#include <haildb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DATABASE "test"
#define TABLE "t"

static const char *data_file_path = "ibdata1:128M:autoextend";



static ib_err_t
create_database(const char* name)
{
    ib_bool_t err;

    err = ib_database_create(name);
    assert(err == IB_TRUE);
    return DB_SUCCESS;
}

static void
create_directory(const char* path)
{
    int ret;

    ret = mkdir(path, S_IRWXU);
    if(ret == -1 && errno != EEXIST) {
        perror(path);
        exit(EXIT_FAILURE);
    }
}

void
test_configure(void)
{
    ib_err_t err;

    create_directory("/tmp/log");

    err = ib_cfg_set_text("flush_method", "O_DIRECT");
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("log_files_in_group", 2);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("log_file_size", 128 * 1024 * 1024);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("log_buffer_size", 8 * 1024 * 1024);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("buffer_pool_size", 5 * 1024 * 1024);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("additional_mem_pool_size", 4 * 1024 * 1024);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("autoextend_increment", 32);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("flush_log_at_trx_commit", 1);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("file_io_threads", 4);
    assert(err == DB_READONLY);

    err = ib_cfg_set_int("lock_wait_timeout", 60);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_int("open_files", 300);
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_bool_on("doublewrite");
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_bool_on("checksums");
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_bool_on("rollback_on_timeout");
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_bool_on("print_verbose_log");
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_bool_on("file_per_table");
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_text("data_home_dir", "/tmp/");
    assert(err == DB_SUCCESS);

    err = ib_cfg_set_text("log_group_home_dir", "/tmp/log/");

    if (err != DB_SUCCESS) {
        fprintf(stderr,
            "syntax error in log_group_home_dir, or a "
            "wrong number of mirrored log groups\n");
        exit(1);
    }

    err = ib_cfg_set_text("data_file_path", data_file_path);

    if (err != DB_SUCCESS) {
        fprintf(stderr,
            "InnoDB: syntax error in data_file_path\n");
        exit(1);
    }
}

static ib_err_t
create_table(const char* dbname, const char* name)
{
    ib_trx_t ib_trx;
    ib_id_t table_id = 0;
    ib_err_t err = DB_SUCCESS;
    ib_tbl_sch_t ib_tbl_sch = NULL;
    ib_idx_sch_t ib_idx_sch = NULL;

    char table_name[IB_MAX_TABLE_NAME_LEN];

    snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);

    err = ib_table_schema_create(table_name, &ib_tbl_sch, IB_TBL_COMPACT, 0);

    assert(err == DB_SUCCESS);

    err = ib_table_schema_add_col(ib_tbl_sch, "id", IB_INT, IB_COL_UNSIGNED, 0, sizeof(ib_u64_t));
    assert(err == DB_SUCCESS);

    err = ib_table_schema_add_col(ib_tbl_sch, "c1", IB_VARCHAR, IB_COL_NONE, 0, 32);
    assert(err == DB_SUCCESS);

    err = ib_table_schema_add_col(ib_tbl_sch, "c2", IB_VARCHAR, IB_COL_NONE, 0, 32);
    assert(err == DB_SUCCESS);

    err = ib_table_schema_add_col(ib_tbl_sch, "c3", IB_INT, IB_COL_UNSIGNED, 0, sizeof(ib_u64_t));
    assert(err == DB_SUCCESS);

    err = ib_table_schema_add_index(ib_tbl_sch, "id", &ib_idx_sch);
    assert(err == DB_SUCCESS);

    err = ib_index_schema_add_col(ib_idx_sch, "id", 0);
    assert(err == DB_SUCCESS);

    err = ib_index_schema_set_clustered(ib_idx_sch);
    assert(err == DB_SUCCESS);

    ib_trx = ib_trx_begin(IB_TRX_REPEATABLE_READ);
    err = ib_schema_lock_exclusive(ib_trx);
    assert(err == DB_SUCCESS);

    err = ib_table_create(ib_trx, ib_tbl_sch, &table_id);
    assert(err == DB_SUCCESS);

    err = ib_trx_commit(ib_trx);
    assert(err == DB_SUCCESS);

    if (ib_tbl_sch != NULL) {
        ib_table_schema_delete(ib_tbl_sch);
    }

    return(err);
}

static ib_err_t
open_table(const char* dbname, const char* name, ib_trx_t ib_trx, ib_crsr_t* crsr)
{
    ib_err_t err = DB_SUCCESS;
    char table_name[IB_MAX_TABLE_NAME_LEN];

    snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);

    err = ib_cursor_open_table(table_name, ib_trx, crsr);
    assert(err == DB_SUCCESS);

    return(err);
}

static ib_err_t
insert_rows(ib_crsr_t crsr)
{
    uint64_t i = 0;
    ib_tpl_t tpl = NULL;
    ib_err_t err = DB_ERROR;

    tpl = ib_clust_read_tuple_create(crsr);
    assert(tpl != NULL);

    for (i = 0; i < 1000000; i++) {
        err = ib_col_set_value(tpl, 0, &i, sizeof(ib_u64_t));
        assert(err == DB_SUCCESS);

        err = ib_col_set_value(tpl, 1, "A", 1);
        assert(err == DB_SUCCESS);

        err = ib_col_set_value(tpl, 2, "B", 2);
        assert(err == DB_SUCCESS);

        err = ib_col_set_value(tpl, 3, &i, sizeof(ib_u64_t));
        assert(err == DB_SUCCESS);

        err = ib_cursor_insert_row(crsr, tpl);
        assert(err == DB_SUCCESS);

        tpl = ib_tuple_clear(tpl);
        assert(tpl != NULL);
    }

    if (tpl != NULL) {
        ib_tuple_delete(tpl);
    }

    return(err);
}

int
main(void)
{
    ib_err_t err;
    ib_crsr_t crsr;
    ib_trx_t ib_trx;

    err = ib_init();
    assert(err == DB_SUCCESS);

    test_configure();

    err = ib_startup("barracuda");
    assert(err == DB_SUCCESS);

    err = create_database(DATABASE);
    assert(err == DB_SUCCESS);

    err = create_table(DATABASE, TABLE);
    assert(err == DB_SUCCESS);

    printf("Begin transaction\n");
    ib_trx = ib_trx_begin(IB_TRX_REPEATABLE_READ);
    assert(ib_trx != NULL);

    printf("Open cursor\n");
    err = open_table(DATABASE, TABLE, ib_trx, &crsr);
    assert(err == DB_SUCCESS);

    printf("Lock table in IX\n");
    err = ib_cursor_lock(crsr, IB_LOCK_IX);
    assert(err == DB_SUCCESS);

    printf("Insert rows\n");
    err = insert_rows(crsr);
    assert(err == DB_SUCCESS);

    printf("Close cursor\n");
    err = ib_cursor_close(crsr);
    assert(err == DB_SUCCESS);
    crsr = NULL;

    printf("Commit transaction\n");
    err = ib_trx_commit(ib_trx);
    assert(err == DB_SUCCESS);

    err = ib_shutdown(IB_SHUTDOWN_NORMAL);
    assert(err == DB_SUCCESS);
    return 0;
}
