/************************************************

  gdbm.c -

  $Author$
  $Date$
  modified at: Mon Jan 24 15:59:52 JST 1994

************************************************/

#include "ruby.h"

#include <gdbm.h>
#include <fcntl.h>
#include <errno.h>

VALUE cGDBM, rb_eGDBMError;

#define MY_BLOCK_SIZE (2048)
#define MY_FATAL_FUNC (0)

struct dbmdata {
    int  di_size;
    GDBM_FILE di_dbm;
};

static void
closed_dbm()
{
    rb_raise(rb_eRuntimeError, "closed GDBM file");
}

#define GetDBM(obj, dbmp) {\
    Data_Get_Struct(obj, struct dbmdata, dbmp);\
    if (dbmp == 0) closed_dbm();\
    if (dbmp->di_dbm == 0) closed_dbm();\
}

static void
free_dbm(dbmp)
    struct dbmdata *dbmp;
{
    if (dbmp) {
	if (dbmp->di_dbm) gdbm_close(dbmp->di_dbm);
	free(dbmp);
    }
}

static VALUE fgdbm_close _((VALUE));

static VALUE
fgdbm_s_new(argc, argv, klass)
    int argc;
    VALUE *argv;
    VALUE klass;
{
    VALUE obj = Data_Wrap_Struct(klass, 0, free_dbm, 0);
    rb_obj_call_init(obj, argc, argv);
    return obj;
}

static VALUE
fgdbm_initialize(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE file, vmode, vflags;
    GDBM_FILE dbm;
    struct dbmdata *dbmp;
    int mode, flags = 0;

    if (rb_scan_args(argc, argv, "12", &file, &vmode, &vflags) == 1) {
	mode = 0666;		/* default value */
    }
    else if (NIL_P(vmode)) {
	mode = -1;		/* return nil if DB not exist */
    }
    else {
	mode = NUM2INT(vmode);
    }

    if (!NIL_P(vflags))
        flags = NUM2INT(vflags);

    Check_SafeStr(file);

    dbm = 0;
    if (mode >= 0)
	dbm = gdbm_open(RSTRING(file)->ptr, MY_BLOCK_SIZE, 
			GDBM_WRCREAT|flags, mode, MY_FATAL_FUNC);
    if (!dbm)
	dbm = gdbm_open(RSTRING(file)->ptr, MY_BLOCK_SIZE, 
			GDBM_WRITER|flags, 0, MY_FATAL_FUNC);
    if (!dbm)
	dbm = gdbm_open(RSTRING(file)->ptr, MY_BLOCK_SIZE, 
			GDBM_READER|flags, 0, MY_FATAL_FUNC);

    if (!dbm) {
	if (mode == -1) return Qnil;

	if (gdbm_errno == GDBM_FILE_OPEN_ERROR ||
	    gdbm_errno == GDBM_CANT_BE_READER ||
	    gdbm_errno == GDBM_CANT_BE_WRITER)
	    rb_sys_fail(RSTRING(file)->ptr);
	else
	    rb_raise(rb_eGDBMError, "%s", gdbm_strerror(gdbm_errno));
    }

    dbmp = ALLOC(struct dbmdata);
    DATA_PTR(obj) = dbmp;
    dbmp->di_dbm = dbm;
    dbmp->di_size = -1;

    return obj;
}

static VALUE
fgdbm_s_open(argc, argv, klass)
    int argc;
    VALUE *argv;
    VALUE klass;
{
    VALUE obj = Data_Wrap_Struct(klass, 0, free_dbm, 0);

    if (NIL_P(fgdbm_initialize(argc, argv, obj))) {
	return Qnil;
    }

    if (rb_block_given_p()) {
        return rb_ensure(rb_yield, obj, fgdbm_close, obj);
    }

    return obj;
}

static VALUE
fgdbm_close(obj)
    VALUE obj;
{
    struct dbmdata *dbmp;

    GetDBM(obj, dbmp);
    gdbm_close(dbmp->di_dbm);
    dbmp->di_dbm = 0;

    return Qnil;
}

static datum
gdbm_fetch_1(dbm, key)
    GDBM_FILE dbm;
    datum key;
{
    static char *ptr;
    datum val;

    val = gdbm_fetch(dbm, key);
    if (ptr) free(ptr);
    ptr = val.dptr;

    return val;
}

static VALUE
fgdbm_fetch(obj, keystr, ifnone)
    VALUE obj, keystr, ifnone;
{
    datum key, value;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    Check_Type(keystr, T_STRING);
    key.dptr = RSTRING(keystr)->ptr;
    key.dsize = RSTRING(keystr)->len;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    value = gdbm_fetch_1(dbm, key);
    if (value.dptr == 0) {
	if (ifnone == Qnil && rb_block_given_p())
	    return rb_yield(rb_tainted_str_new(key.dptr, key.dsize));
	return ifnone;
    }
    return rb_tainted_str_new(value.dptr, value.dsize);
}

static VALUE
fgdbm_aref(obj, keystr)
    VALUE obj, keystr;
{
    return fgdbm_fetch(obj, keystr, Qnil);
}

static VALUE
fgdbm_fetch_m(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE keystr, valstr, ifnone;

    rb_scan_args(argc, argv, "11", &keystr, &ifnone);
    valstr = fgdbm_fetch(obj, keystr, ifnone);
    if (argc == 1 && !rb_block_given_p() && NIL_P(valstr))
	rb_raise(rb_eIndexError, "key not found");

    return valstr;
}

static VALUE
fgdbm_index(obj, valstr)
    VALUE obj, valstr;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    Check_Type(valstr, T_STRING);
    val.dptr = RSTRING(valstr)->ptr;
    val.dsize = RSTRING(valstr)->len;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	if (val.dsize == RSTRING(valstr)->len &&
	    memcmp(val.dptr, RSTRING(valstr)->ptr, val.dsize) == 0)
	    return rb_tainted_str_new(key.dptr, key.dsize);
    }
    return Qnil;
}

static VALUE
fgdbm_indexes(argc, argv, obj)
    int argc;
    VALUE *argv;
    VALUE obj;
{
    VALUE new;
    int i;

    new = rb_ary_new2(argc);
    for (i=0; i<argc; i++) {
	rb_ary_push(new, fgdbm_fetch(obj, argv[i]));
    }

    return new;
}

static VALUE
fgdbm_delete(obj, keystr)
    VALUE obj, keystr;
{
    datum key;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    rb_secure(4);
    Check_Type(keystr, T_STRING);
    key.dptr = RSTRING(keystr)->ptr;
    key.dsize = RSTRING(keystr)->len;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    if (!gdbm_exists(dbm, key)) {
	if (rb_block_given_p()) rb_yield(keystr);
	return Qnil;
    }

    if (gdbm_delete(dbm, key)) {
	dbmp->di_size = -1;
	rb_raise(rb_eGDBMError, "%s", gdbm_strerror(gdbm_errno));
    }
    else if (dbmp->di_size >= 0) {
	dbmp->di_size--;
    }
    return obj;
}

static VALUE
fgdbm_shift(obj)
    VALUE obj;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE keystr, valstr;

    rb_secure(4);
    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    key = gdbm_firstkey(dbm); 
    if (!key.dptr) return Qnil;
    val = gdbm_fetch_1(dbm, key);
    gdbm_delete(dbm, key);

    keystr = rb_tainted_str_new(key.dptr, key.dsize);
    valstr = rb_tainted_str_new(val.dptr, val.dsize);
    return rb_assoc_new(keystr, valstr);
}

static VALUE
fgdbm_delete_if(obj)
    VALUE obj;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE keystr, valstr;

    rb_secure(4);
    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	keystr = rb_tainted_str_new(key.dptr, key.dsize);
	valstr = rb_tainted_str_new(val.dptr, val.dsize);
	if (RTEST(rb_yield(rb_assoc_new(keystr, valstr)))) {
	    if (gdbm_delete(dbm, key)) {
		rb_raise(rb_eGDBMError, "%s", gdbm_strerror(gdbm_errno));
	    }
	}
    }
    return obj;
}

static VALUE
fgdbm_clear(obj)
    VALUE obj;
{
    datum key, nextkey;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    rb_secure(4);
    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    dbmp->di_size = -1;
    for (key = gdbm_firstkey(dbm); key.dptr; key = nextkey) {
	nextkey = gdbm_nextkey(dbm, key);
	if (gdbm_delete(dbm, key)) {
	    rb_raise(rb_eGDBMError, "%s", gdbm_strerror(gdbm_errno));
	}
    }
    return obj;
}

static VALUE
fgdbm_invert(obj)
    VALUE obj;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE keystr, valstr;
    VALUE hash = rb_hash_new();

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	keystr = rb_tainted_str_new(key.dptr, key.dsize);
	valstr = rb_tainted_str_new(val.dptr, val.dsize);
	rb_hash_aset(hash, valstr, keystr);
    }
    return hash;
}

static VALUE
each_pair(obj)
    VALUE obj;
{
    return rb_funcall(obj, rb_intern("each_pair"), 0, 0);
}

static VALUE fgdbm_store _((VALUE,VALUE,VALUE));

static VALUE
update_i(pair, dbm)
    VALUE pair, dbm;
{
    Check_Type(pair, T_ARRAY);
    if (RARRAY(pair)->len < 2) {
	rb_raise(rb_eArgError, "pair must be [key, value]");
    }
    fgdbm_store(dbm, RARRAY(pair)->ptr[0], RARRAY(pair)->ptr[1]);
    return Qnil;
}

static VALUE
fgdbm_update(obj, other)
    VALUE obj, other;
{
    rb_iterate(each_pair, other, update_i, obj);
    return obj;
}

static VALUE
fgdbm_replace(obj, other)
    VALUE obj, other;
{
    fgdbm_clear(obj);
    rb_iterate(each_pair, other, update_i, obj);
    return obj;
}

static VALUE
fgdbm_store(obj, keystr, valstr)
    VALUE obj, keystr, valstr;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    rb_secure(4);
    keystr = rb_obj_as_string(keystr);

    key.dptr = RSTRING(keystr)->ptr;
    key.dsize = RSTRING(keystr)->len;

    valstr = rb_obj_as_string(valstr);
    val.dptr = RSTRING(valstr)->ptr;
    val.dsize = RSTRING(valstr)->len;

    GetDBM(obj, dbmp);
    dbmp->di_size = -1;
    dbm = dbmp->di_dbm;
    if (gdbm_store(dbm, key, val, GDBM_REPLACE)) {
	if (errno == EPERM) rb_sys_fail(0);
	rb_raise(rb_eGDBMError, "%s", gdbm_strerror(gdbm_errno));
    }

    return valstr;
}

static VALUE
fgdbm_length(obj)
    VALUE obj;
{
    datum key;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    int i = 0;

    GetDBM(obj, dbmp);
    if (dbmp->di_size > 0) return INT2FIX(dbmp->di_size);
    dbm = dbmp->di_dbm;

    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	i++;
    }
    dbmp->di_size = i;

    return INT2FIX(i);
}

static VALUE
fgdbm_empty_p(obj)
    VALUE obj;
{
    datum key;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    int i = 0;

    GetDBM(obj, dbmp);
    if (dbmp->di_size < 0) {
	dbm = dbmp->di_dbm;

	for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	    i++;
	}
    }
    else {
	i = dbmp->di_size;
    }
    if (i == 0) return Qtrue;
    return Qfalse;
}

static VALUE
fgdbm_each_value(obj)
    VALUE obj;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	rb_yield(rb_tainted_str_new(val.dptr, val.dsize));
    }
    return obj;
}

static VALUE
fgdbm_each_key(obj)
    VALUE obj;
{
    datum key;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	rb_yield(rb_tainted_str_new(key.dptr, key.dsize));
    }
    return obj;
}

static VALUE
fgdbm_each_pair(obj)
    VALUE obj;
{
    datum key, val;
    GDBM_FILE dbm;
    struct dbmdata *dbmp;
    VALUE keystr, valstr;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	keystr = rb_tainted_str_new(key.dptr, key.dsize);
	valstr = rb_tainted_str_new(val.dptr, val.dsize);
	rb_yield(rb_assoc_new(keystr, valstr));
    }

    return obj;
}

static VALUE
fgdbm_keys(obj)
    VALUE obj;
{
    datum key;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE ary;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    ary = rb_ary_new();
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	rb_ary_push(ary, rb_tainted_str_new(key.dptr, key.dsize));
    }

    return ary;
}

static VALUE
fgdbm_values(obj)
    VALUE obj;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE ary;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    ary = rb_ary_new();
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	rb_ary_push(ary, rb_tainted_str_new(val.dptr, val.dsize));
    }

    return ary;
}

static VALUE
fgdbm_has_key(obj, keystr)
    VALUE obj, keystr;
{
    datum key;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    Check_Type(keystr, T_STRING);
    key.dptr = RSTRING(keystr)->ptr;
    key.dsize = RSTRING(keystr)->len;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    if (gdbm_exists(dbm, key))
        return Qtrue;
    return Qfalse;
}

static VALUE
fgdbm_has_value(obj, valstr)
    VALUE obj, valstr;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    Check_Type(valstr, T_STRING);
    val.dptr = RSTRING(valstr)->ptr;
    val.dsize = RSTRING(valstr)->len;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	if (val.dsize == RSTRING(valstr)->len &&
	    memcmp(val.dptr, RSTRING(valstr)->ptr, val.dsize) == 0)
	    return Qtrue;
    }
    return Qfalse;
}

static VALUE
fgdbm_to_a(obj)
    VALUE obj;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE ary;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    ary = rb_ary_new();
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	rb_ary_push(ary, rb_assoc_new(rb_tainted_str_new(key.dptr, key.dsize),
				rb_tainted_str_new(val.dptr, val.dsize)));
    }

    return ary;
}

static VALUE
fgdbm_reorganize(obj)
    VALUE obj;
{
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    rb_secure(4);
    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    gdbm_reorganize(dbm);
    return obj;
}

static VALUE
fgdbm_sync(obj)
    VALUE obj;
{
    struct dbmdata *dbmp;
    GDBM_FILE dbm;

    rb_secure(4);
    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;
    gdbm_sync(dbm);
    return obj;
}

static VALUE
fgdbm_set_cachesize(obj, val)
    VALUE obj, val;
{
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE hash;
    int optval;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    optval = FIX2INT(val);
    if (gdbm_setopt(dbm, GDBM_CACHESIZE, &optval, sizeof(optval)) == -1) {
	rb_raise(rb_eGDBMError, "%s", gdbm_strerror(gdbm_errno));
    }
    return val;
}

static VALUE
fgdbm_set_fastmode(obj, val)
    VALUE obj, val;
{
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE hash;
    int optval;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    optval = 0;
    if (RTEST(val))
        optval = 1;

    if (gdbm_setopt(dbm, GDBM_FASTMODE, &optval, sizeof(optval)) == -1) {
	rb_raise(rb_eGDBMError, "%s", gdbm_strerror(gdbm_errno));
    }
    return val;
}

static VALUE
fgdbm_set_syncmode(obj, val)
    VALUE obj, val;
{
#if !defined(GDBM_SYNCMODE)
    fgdbm_set_fastmode(obj, RTEST(val) ? Qfalse : Qtrue);
    return val;
#else
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE hash;
    int optval;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    optval = 0;
    if (RTEST(val))
        optval = 1;

    if (gdbm_setopt(dbm, GDBM_FASTMODE, &optval, sizeof(optval)) == -1) {
	rb_raise(rb_eGDBMError, "%s", gdbm_strerror(gdbm_errno));
    }
    return val;
#endif
}

static VALUE
fgdbm_to_hash(obj)
    VALUE obj;
{
    datum key, val;
    struct dbmdata *dbmp;
    GDBM_FILE dbm;
    VALUE hash;

    GetDBM(obj, dbmp);
    dbm = dbmp->di_dbm;

    hash = rb_hash_new();
    for (key = gdbm_firstkey(dbm); key.dptr; key = gdbm_nextkey(dbm, key)) {
	val = gdbm_fetch_1(dbm, key);
	rb_hash_aset(hash, rb_tainted_str_new(key.dptr, key.dsize),
		           rb_tainted_str_new(val.dptr, val.dsize));
    }

    return hash;
}

static VALUE
fgdbm_reject(obj)
    VALUE obj;
{
    return rb_hash_delete_if(fgdbm_to_hash(obj));
}

void
Init_gdbm()
{
    cGDBM = rb_define_class("GDBM", rb_cObject);
    rb_eGDBMError = rb_define_class("GDBMError", rb_eStandardError);
    rb_include_module(cGDBM, rb_mEnumerable);

    rb_define_singleton_method(cGDBM, "new", fgdbm_s_new, -1);
    rb_define_singleton_method(cGDBM, "open", fgdbm_s_open, -1);

    rb_define_method(cGDBM, "initialize", fgdbm_initialize, -1);
    rb_define_method(cGDBM, "close", fgdbm_close, 0);
    rb_define_method(cGDBM, "[]", fgdbm_aref, 1);
    rb_define_method(cGDBM, "fetch", fgdbm_fetch_m, -1);
    rb_define_method(cGDBM, "[]=", fgdbm_store, 2);
    rb_define_method(cGDBM, "store", fgdbm_store, 2);
    rb_define_method(cGDBM, "index",  fgdbm_index, 1);
    rb_define_method(cGDBM, "indexes",  fgdbm_indexes, -1);
    rb_define_method(cGDBM, "indices",  fgdbm_indexes, -1);
    rb_define_method(cGDBM, "length", fgdbm_length, 0);
    rb_define_alias(cGDBM,  "size", "length");
    rb_define_method(cGDBM, "empty?", fgdbm_empty_p, 0);
    rb_define_method(cGDBM, "each", fgdbm_each_pair, 0);
    rb_define_method(cGDBM, "each_value", fgdbm_each_value, 0);
    rb_define_method(cGDBM, "each_key", fgdbm_each_key, 0);
    rb_define_method(cGDBM, "each_pair", fgdbm_each_pair, 0);
    rb_define_method(cGDBM, "keys", fgdbm_keys, 0);
    rb_define_method(cGDBM, "values", fgdbm_values, 0);
    rb_define_method(cGDBM, "shift", fgdbm_shift, 1);
    rb_define_method(cGDBM, "delete", fgdbm_delete, 1);
    rb_define_method(cGDBM, "delete_if", fgdbm_delete_if, 0);
    rb_define_method(cGDBM, "reject!", fgdbm_delete_if, 0);
    rb_define_method(cGDBM, "reject", fgdbm_reject, 0);
    rb_define_method(cGDBM, "clear", fgdbm_clear, 0);
    rb_define_method(cGDBM,"invert", fgdbm_invert, 0);
    rb_define_method(cGDBM,"update", fgdbm_update, 1);
    rb_define_method(cGDBM,"replace", fgdbm_replace, 1);
    rb_define_method(cGDBM,"reorganize", fgdbm_reorganize, 0);
    rb_define_method(cGDBM,"sync", fgdbm_sync, 0);
    /* rb_define_method(cGDBM,"setopt", fgdbm_setopt, 2); */
    rb_define_method(cGDBM,"cachesize=", fgdbm_set_cachesize, 1);
    rb_define_method(cGDBM,"fastmode=", fgdbm_set_fastmode, 1);
    rb_define_method(cGDBM,"syncmode=", fgdbm_set_syncmode, 1);

    rb_define_method(cGDBM, "include?", fgdbm_has_key, 1);
    rb_define_method(cGDBM, "has_key?", fgdbm_has_key, 1);
    rb_define_method(cGDBM, "member?", fgdbm_has_key, 1);
    rb_define_method(cGDBM, "has_value?", fgdbm_has_value, 1);
    rb_define_method(cGDBM, "key?", fgdbm_has_key, 1);
    rb_define_method(cGDBM, "value?", fgdbm_has_value, 1);

    rb_define_method(cGDBM, "to_a", fgdbm_to_a, 0);
    rb_define_method(cGDBM, "to_hash", fgdbm_to_hash, 0);

    /* flags for gdbm_opn() */
    /*
    rb_define_const(cGDBM, "READER",  INT2FIX(GDBM_READER));
    rb_define_const(cGDBM, "WRITER",  INT2FIX(GDBM_WRITER));
    rb_define_const(cGDBM, "WRCREAT", INT2FIX(GDBM_WRCREAT));
    rb_define_const(cGDBM, "NEWDB",   INT2FIX(GDBM_NEWDB));
    */
    rb_define_const(cGDBM, "FAST", INT2FIX(GDBM_FAST));
    /* this flag is obsolete in gdbm 1.8.
       On gdbm 1.8, fast mode is default behavior. */

    /* gdbm version 1.8 specific */
#if defined(GDBM_SYNC)
    rb_define_const(cGDBM, "SYNC",    INT2FIX(GDBM_SYNC));
#endif
#if defined(GDBM_NOLOCK)
    rb_define_const(cGDBM, "NOLOCK",  INT2FIX(GDBM_NOLOCK));
#endif
    rb_define_const(cGDBM, "VERSION",  rb_str_new2(gdbm_version));
}