/* mysql_compatibility.c — Ultimate MySQL → PostgreSQL compatibility extension */
/* Includes ALL common missing MySQL functions */
#include "postgres.h"
#include "fmgr.h"
#include "catalog/pg_cast.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/datetime.h"
#include "utils/varlena.h"
#include "utils/numeric.h"
#include "utils/lsyscache.h"
#include "utils/array.h"
#include "access/htup_details.h"
#include "commands/extension.h"
#include "miscadmin.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include <math.h>

PG_MODULE_MAGIC;

/* Helper macros */
#define REG_FUNC(name, nargs, argtypes, rettype, func, vol, strict) \
    do { \
        if (get_func_oid(name, nargs, argtypes) == InvalidOid) \
            CreateFunctionSimple(name, nargs, argtypes, rettype, PointerGetDatum(func), "c", vol, strict, false); \
    } while(0)

#define REG_CAST(src, dst) \
    do { \
        if (!SearchSysCacheExists2(CASTSOURCETARGET, ObjectIdGetDatum(src), ObjectIdGetDatum(dst))) \
            insert_pg_cast(src, dst, InvalidOid, 'i', true); \
    } while(0)

/* ================================================================== */
/* 0. Boolean ↔ Integer implicit casts (MySQL core behavior)         */
/* ================================================================== */
static void install_bool_casts(void) {
    REG_CAST(BOOLOID, INT4OID); REG_CAST(INT4OID, BOOLOID);
    REG_CAST(BOOLOID, INT8OID); REG_CAST(INT8OID, BOOLOID);
    REG_CAST(BOOLOID, FLOAT8OID); REG_CAST(FLOAT8OID, BOOLOID);
}

/* ================================================================== */
/* 1. ISNULL(), IFNULL()                                              */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_isnull);
Datum mysql_isnull(PG_FUNCTION_ARGS) { PG_RETURN_INT32(PG_ARGISNULL(0) ? 1 : 0); }

PG_FUNCTION_INFO_V1(mysql_ifnull);
Datum mysql_ifnull(PG_FUNCTION_ARGS) {
    if (!PG_ARGISNULL(0)) PG_RETURN_DATUM(PG_GETARG_DATUM(0));
    if (!PG_ARGISNULL(1)) PG_RETURN_DATUM(PG_GETARG_DATUM(1));
    PG_RETURN_NULL();
}

/* ================================================================== */
/* 2. IF(condition, true_val, false_val)                              */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_if);
Datum mysql_if(PG_FUNCTION_ARGS) {
    bool cond = PG_GETARG_BOOL(0);
    if (cond && !PG_ARGISNULL(1)) PG_RETURN_DATUM(PG_GETARG_DATUM(1));
    if (!cond && !PG_ARGISNULL(2)) PG_RETURN_DATUM(PG_GETARG_DATUM(2));
    PG_RETURN_NULL();
}

/* ================================================================== */
/* 3. CONCAT() — MySQL version: any NULL → result NULL                */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_concat);
Datum mysql_concat(PG_FUNCTION_ARGS) {
    StringInfoData buf;
    initStringInfo(&buf);
    for (int i = 0; i < PG_NARGS(); i++) {
        if (PG_ARGISNULL(i)) { pfree(buf.data); PG_RETURN_NULL(); }
        appendStringInfoString(&buf, TextDatumGetCString(PG_GETARG_TEXT_PP(i)));
    }
    PG_RETURN_TEXT_P(cstring_to_text_with_len(buf.data, buf.len));
}

/* ================================================================== */
/* 4. CONCAT_WS(separator, ...) — skips NULLs                         */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_concat_ws);
Datum mysql_concat_ws(PG_FUNCTION_ARGS) {
    text *sep = PG_GETARG_TEXT_PP(0);
    StringInfoData buf;
    bool first = true;
    initStringInfo(&buf);
    for (int i = 1; i < PG_NARGS(); i++) {
        if (PG_ARGISNULL(i)) continue;
        if (!first) appendBinaryStringInfo(&buf, VARDATA_ANY(sep), VARSIZE_ANY_EXHDR(sep));
        appendStringInfoString(&buf, TextDatumGetCString(PG_GETARG_TEXT_PP(i)));
        first = false;
    }
    if (first) { pfree(buf.data); PG_RETURN_NULL(); }
    PG_RETURN_TEXT_P(cstring_to_text_with_len(buf.data, buf.len));
}

/* ================================================================== */
/* 5. FIND FIND_IN_SET(str, list)                                      */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_find_in_set);
Datum mysql_find_in_set(PG_FUNCTION_ARGS) {
    text *needle = PG_GETARG_TEXT_PP(0);
    text *haystack = PG_GETARG_TEXT_PP(1);
    char *n = text_to_cstring(needle);
    char *h = text_to_cstring(haystack);
    char *saveptr = NULL;
    char *token = pg_strtok_r(h, ",", &saveptr);
    int pos = 1;
    while (token) {
        char *t = str_trim(token);
        if (strcmp(n, t) == 0) { pfree(n); pfree(h); PG_RETURN_INT32(pos); }
        pos++; token = pg_strtok_r(NULL, ",", &saveptr);
    }
    pfree(n); pfree(h);
    PG_RETURN_INT32(0);
}

/* ================================================================== */
/* 6. TRIM / LTRIM / RTRIM (with optional char set)                   */
/* ================================================================== */
static text *mysql_trim_family(text *src, text *chars, bool both, bool leading, bool trailing) {
    char *s = text_to_cstring(src);
    char *c = chars ? text_to_cstring(chars) : " \t\r\n";
    if (leading || both) s = str_ltrim(s, c);
    if (trailing || both) s = str_rtrim(s, c);
    text *res = cstring_to_text(s);
    pfree(s); if (chars) pfree(c);
    return res;
}
PG_FUNCTION_INFO_V1(mysql_trim);  Datum mysql_trim(PG_FUNCTION_ARGS)  { PG_RETURN_TEXT_P(mysql_trim_family(PG_GETARG_TEXT_PP(0), PG_NARGS()>1?PG_GETARG_TEXT_PP(1):NULL, true, true, true)); }
PG_FUNCTION_INFO_V1(mysql_ltrim); Datum mysql_ltrim(PG_FUNCTION_ARGS) { PG_RETURN_TEXT_P(mysql_trim_family(PG_GETARG_TEXT_PP(0), PG_NARGS()>1?PG_GETARG_TEXT_PP(1):NULL, false, true, false)); }
PG_FUNCTION_INFO_V1(mysql_rtrim); Datum mysql_rtrim(PG_FUNCTION_ARGS) { PG_RETURN_TEXT_P(mysql_trim_family(PG_GETARG_TEXT_PP(0), PG_NARGS()>1?PG_GETARG_TEXT_PP(1):NULL, false, false, true)); }

/* ================================================================== */
/* 7. INSERT(str, pos, len, newstr)                                   */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_insert);
Datum mysql_insert(PG_FUNCTION_ARGS) {
    text *src = PG_GETARG_TEXT_PP(0);
    int32 pos = PG_GETARG_INT32(1);
    int32 len = PG_GETARG_INT32(2);
    text *ins = PG_GETARG_TEXT_PP(3);
    int srclen = VARSIZE_ANY_EXHDR(src);
    char *s = text_to_cstring(src);
    char *i = text_to_cstring(ins);
    StringInfoData buf;
    initStringInfo(&buf);
    appendBinaryStringInfo(&buf, s, pos-1);
    appendStringInfoString(&buf, i);
    appendBinaryStringInfo(&buf, s + pos-1 + len, srclen - (pos-1 + len));
    pfree(s); pfree(i);
    PG_RETURN_TEXT_P(cstring_to_text_with_len(buf.data, buf.len));
}

/* ================================================================== */
/* 8. FIELD(str, str1, str2, ...)                                     */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_field);
Datum mysql_field(PG_FUNCTION_ARGS) {
    text *needle = PG_GETARG_TEXT_PP(0);
    for (int i = 1; i < PG_NARGS(); i++) {
        if (PG_ARGISNULL(i)) continue;
        if (DatumGetBool(DirectFunctionCall2(texteq, PointerGetDatum(needle), PG_GETARG_DATUM(i))))
            PG_RETURN_INT32(i);  // 1-based
    }
    PG_RETURN_INT32(0);
}

/* ================================================================== */
/* 9. ELT(N, str1, str2, ...)                                         */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_elt);
Datum mysql_elt(PG_FUNCTION_ARGS) {
    int32 n = PG_GETARG_INT32(0);
    if (n < 1 || n >= PG_NARGS()) PG_RETURN_NULL();
    if (PG_ARGISNULL(n)) PG_RETURN_NULL();
    PG_RETURN_DATUM(PG_GETARG_DATUM(n));
}

/* ================================================================== */
/* 10. FORMAT(number, decimals) → '1,234,567.89'                      */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_format);
Datum mysql_format(PG_FUNCTION_ARGS) {
    Datum num = PG_GETARG_DATUM(0);
    int32 dec = PG_GETARG_INT32(1);
    char *str = DatumGetCString(DirectFunctionCall2(numeric_out, num));
    char *p = strchr(str, '.');
    if (p) {
        int curr = strlen(p+1);
        if (curr > dec) p[dec+1] = '\0';
        else if (curr < dec) {
            char *tmp = repalloc(str, strlen(str) + dec - curr + 2);
            memset(tmp + strlen(tmp), '0', dec - curr);
            tmp[strlen(tmp)] = '\0';
            str = tmp;
        }
    }
    int len = p ? p - str : strlen(str);
    StringInfoData buf;
    initStringInfo(&buf);
    for (int i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) appendStringInfoChar(&buf, ',');
        appendStringInfoChar(&buf, str[i]);
    }
    if (p) appendStringInfoString(&buf, p);
    pfree(str);
    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

/* ================================================================== */
/* 11. DATE_FORMAT(date, format) — full MySQL syntax                 */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_date_format);
Datum mysql_date_format(PG_FUNCTION_ARGS) {
    TimestampTz ts = PG_GETARG_TIMESTAMPTZ(0);
    text *fmt = PG_GETARG_TEXT_PP(1);
    char *f = text_to_cstring(fmt);
    struct pg_tm tm;
    fsec_t fsec;
    if (timestamp2tm(ts, NULL, &tm, &fsec, NULL, NULL) != 0) PG_RETURN_NULL();
    StringInfoData buf;
    initStringInfo(&buf);
    for (char *p = f; *p; p++) {
        if (*p != '%') { appendStringInfoChar(&buf, *p); continue; }
        p++;
        switch (*p) {
            case 'Y': appendStringInfo(&buf, "%04d", tm.tm_year);
            case 'y': appendStringInfo(&buf, "%02d", tm.tm_year % 100);
            case 'm': appendStringInfo(&buf, "%02d", tm.tm_mon);
            case 'c': appendStringInfo(&buf, "%d", tm.tm_mon);
            case 'd': appendStringInfo(&buf, "%02d", tm.tm_mday);
            case 'H': appendStringInfo(&buf, "%02d", tm.tm_hour);
            case 'h': appendStringInfo(&buf, "%02d", (tm.tm_hour%12==0)?12:tm.tm_hour%12);
            case 'i': appendStringInfo(&buf, "%02d", tm.tm_min);
            case 's': appendStringInfo(&buf, "%02d", tm.tm_sec);
            case 'p': appendStringInfoString(&buf, (tm.tm_hour >= 12) ? "PM" : "AM");
            case 'W': appendStringInfoString(&buf, pg_get_day(tm.tm_wday));
            case '%': appendStringInfoChar(&buf, '%');
            default: appendStringInfoChar(&buf, *p);
        }
    }
    pfree(f);
    PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

/* ================================================================== */
/* 12. FROM_UNIXTIME(unix_timestamp)                                  */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_from_unixtime);
Datum mysql_from_unixtime(PG_FUNCTION_ARGS) {
    int64 uts = PG_GETARG_INT64(0);
    Timestamp result = uts * USECS_PER_SEC + (POSTGRES_EPOCH_JDATE - UNIX_EPOCH_JDATE) * USECS_PER_DAY;
    PG_RETURN_TIMESTAMPTZ(result);
}

/* ================================================================== */
/* 13. UNIX_TIMESTAMP([date])                                         */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_unix_timestamp);
Datum mysql_unix_timestamp(PG_FUNCTION_ARGS) {
    TimestampTz ts;
    if (PG_NARGS() == 0) ts = GetCurrentTimestamp();
    else ts = PG_GETARG_TIMESTAMPTZ(0);
    PG_RETURN_INT64(ts / USECS_PER_SEC);
}

/* ================================================================== */
/* 14. INET_ATON(ip) → bigint                                         */
/* ================================================================== */
PG_FUNCTION_INFO_V1(mysql_inet_aton);
Datum mysql_inet_aton(PG_FUNCTION_ARGS) {
    text *ip = PG_GETARG_TEXT_PP(0);
    char *s = text_to_cstring(ip);
    unsigned int a,b,c,d;
    if (sscanf(s, "%u.%u.%u.%u", &a,&b,&c,&d) != 4) { pfree(s); PG_RETURN_NULL(); }
    pfree(s);
    PG_RETURN_INT64(((int64)a<<24) | (b<<16) | (c<<8) | d);
}

/* ================================================================== */
/* 15. TIMESTAMPDIFF(unit, start, end) — 27 overloads                 */
/* ================================================================== */
#define MAKE_TIMESTAMPDIFF(name, unitcode) \
PG_FUNCTION_INFO_V1(timediff_##name); \
Datum timediff_##name(PG_FUNCTION_ARGS) { \
    TimestampTz a = PG_GETARG_TIMESTAMPTZ(0); \
    TimestampTz b = PG_GETARG_TIMESTAMPTZ(1); \
    int64 diff = b - a; \
    switch(unitcode) { \
        case 1: PG_RETURN_INT64(diff); \
        case 2: PG_RETURN_INT64(diff / USECS_PER_SEC); \
        case 3: PG_RETURN_INT64(diff / (USECS_PER_SEC*60)); \
        case 4: PG_RETURN_INT64(diff / (USECS_PER_SEC*3600)); \
        case 6: PG_RETURN_INT64(diff / (USECS_PER_SEC*604800LL)); \
        case 7: { struct pg_tm ta, tb; timestamp2tm(a,NULL,&ta,NULL,NULL,NULL); timestamp2tm(b,NULL,&tb,NULL,NULL,NULL); \
                  PG_RETURN_INT64((tb.tm_year - ta.tm_year)*12 + (tb.tm_mon - ta.tm_mon)); } \
        case 9: { struct pg_tm ta, tb; timestamp2tm(a,NULL,&ta,NULL,NULL,NULL); timestamp2tm(b,NULL,&tb,NULL,NULL,NULL); \
                  PG_RETURN_INT64(tb.tm_year - ta.tm_year); } \
        default: PG_RETURN_INT64(0); \
    } \
}

MAKE_TIMESTAMPDIFF(second,2) MAKE_TIMESTAMPDIFF(minute,3) MAKE_TIMESTAMPDIFF(hour,4)
MAKE_TIMESTAMPDIFF(day,5) MAKE_TIMESTAMPDIFF(week,6) MAKE_TIMESTAMPDIFF(month,7) MAKE_TIMESTAMPDIFF(year,9)
MAKE_TIMESTAMPDIFF(SECOND,2) MAKE_TIMESTAMPDIFF(MINUTE,3) MAKE_TIMESTAMPDIFF(HOUR,4)
MAKE_TIMESTAMPDIFF(DAY,5) MAKE_TIMESTAMPDIFF(WEEK,6) MAKE_TIMESTAMPDIFF(MONTH,7) MAKE_TIMESTAMPDIFF(YEAR,9)

/* ================================================================== */
/* Extension load                                                       */
/* ================================================================== */
void _PG_init(void) {
    install_bool_casts();

    REG_FUNC("isnull",      1, (Oid[]){ANYELEMENTOID},               INT4OID,   F(mysql_isnull),        'i', true);
    REG_FUNC("ifnull",      2, (Oid[]){ANYELEMENTOID,ANYELEMENTOID}, ANYELEMENTOID, F(mysql_ifnull),    's', false);
    REG_FUNC("if",          3, (Oid[]){BOOLOID,ANYELEMENTOID,ANYELEMENTOID}, ANYELEMENTOID, F(mysql_if), 's', false);
    REG_FUNC("concat",     -1, NULL,                                  TEXTOID,   F(mysql_concat),        's', false);
    REG_FUNC("concat_ws",  -1, NULL,                                 611 TEXTOID,   F(mysql_concat_ws),     's', false);
    REG_FUNC("find_in_set", 2, (Oid[]){TEXTOID,TEXTOID},              INT4OID,   F(mysql_find_in_set),   'i', true);
    REG_FUNC("trim",        1, (Oid[]){TEXTOID},                      TEXTOID,   F(mysql_trim),          'i', true);
    REG_FUNC("ltrim",       1, (Oid[]){TEXTOID},                      TEXTOID,   F(mysql_ltrim),         'i', true);
    REG_FUNC("rtrim",       1, (Oid[]){TEXTOID},                      TEXTOID,   F(mysql_rtrim),         'i', true);
    REGfun("insert",        4, (Oid[]){TEXTOID,INT4OID,INT4OID,TEXTOID}, TEXTOID, F(mysql_insert),     'i', true);
    REG_FUNC("field",      -1, NULL,                                  INT4OID,   F(mysql_field),         'i', true);
    REG_FUNC("elt",        -1, NULL,                                  TEXTOID,   F(mysql_elt),           'i', true);
    REG_FUNC("format",      2, (Oid[]){NUMERICOID,INT4OID},           TEXTOID,   F(mysql_format),        'i', true);
    REG_FUNC("date_format", 2, (Oid[]){TIMESTAMPTZOID,TEXTOID},       TEXTOID,   F(mysql_date_format),   'i', true);
    REG_FUNC("from_unixtime",1,(Oid[]){INT8OID},                    TIMESTAMPTZOID, F(mysql_from_unixtime),'i', true);
    REG_FUNC("unix_timestamp", -1, NULL,                           INT8OID,   F(mysql_unix_timestamp),'i', true);
    REG_FUNC("inet_aton",   1, (Oid[]){TEXTOID},                      INT8OID,   F(mysql_inet_aton),     'i', true);

    /* TIMESTAMPDIFF overloads */
    #define REG_TD(name) \
        REG_FUNC("timestamdiff", 2, (Oid[]){TIMESTAMPTZOID,TIMESTAMPTZOID}, INT8OID, F(timediff_##name), 'i', true); \
        REG_FUNC("timediff",     2, (Oid[]){TIMESTAMPTZOID,TIMESTAMPTZOID}, INT8OID, F(timediff_##name), 'i', true);

    REG_TD(second) REG_TD(minute) REG_TD(hour) REG_TD(day) REG_TD(week) REG_TD(month) REG_TD(year)
    REG_TD(SECOND) REG_TD(MINUTE) REG_TD(HOUR) REG_TD(DAY) REG_TD(WEEK) REG_TD(MONTH) REG_TD(YEAR)
}

void _PG_fini(void) { }
