
#include "../src/ast.h"
#include "../src/interp/interp.h"
#include "../src/opt/tee.h"
#include "../src/opt/tree.h"
#include "../src/read_bin.h"
#include "../src/read_wat.h"
#include "../src/api/api.h"

web49_env_func_t web49_main_import_func(void *state, const char *mod, const char *func) {
    if (!strcmp(mod, "wasi_snapshot_preview1")) {
        return web49_api_import_wasi(state, func);
    } else {
        return NULL;
    }
}

web49_interp_data_t web49_main_expr_to_data(web49_readwat_expr_t expr) {
    web49_interp_data_t ret;
    if (!strcmp(expr.fun_fun, "i32.const")) {
        ret.i32_s = (int32_t) web49_readwat_expr_to_i64(expr.fun_args[0]);
    }
    return ret;
}

int web49_file_main(const char *inarg, const char **args) {
    web49_io_input_t infile = web49_io_input_open(inarg);
    web49_module_t mod;
    if (memcmp(infile.byte_buf, "\0asm", 4) == 0) {
        mod = web49_readbin_module(&infile);
    } else {
        const char *v = strrchr(inarg, '.');
        if (!strcmp(v, ".wast")) {
            web49_readwat_expr_t expr = web49_readwat_expr(&infile);
            mod = web49_readwat_to_module(expr);
            web49_opt_tee_module(&mod);
            web49_opt_tree_module(&mod);
            web49_interp_t interp = web49_interp_module(mod, args);
            while (true) {
                web49_readwat_expr_t todo = web49_readwat_expr(&infile);
                if (todo.start == todo.end) {
                    break;
                }
                if (todo.tag == WEB49_READWAT_EXPR_TAG_SYM) {
                    if (todo.sym[0] == '\0') {
                        return 0;
                    }
                    fprintf(stderr, "wasm spec test: dont know how to handle: <%s>\n", todo.sym);
                    return 1;
                } else if (todo.tag == WEB49_READWAT_EXPR_TAG_STR) {
                    fprintf(stderr, "wasm spec test: dont know how to handle: \"%.*s\"\n", (int) todo.len_str, todo.str);
                    return 1;
                } else if (!strcmp(todo.fun_fun, "assert_return")) {
                    web49_readwat_expr_t invoke = todo.fun_args[0];
                    web49_readwat_expr_t wants = todo.fun_args[1];
                    if (invoke.tag != WEB49_READWAT_EXPR_TAG_FUN || !!strcmp(invoke.fun_fun, "invoke")) {
                        fprintf(stderr, "wasm spec test: assert returnd eos not look like (assert_return (invoke ...) ...)");
                        return 1;
                    }
                    size_t argno = 0;
                    for (size_t i = 1; i < invoke.fun_nargs; i++) {
                        interp.locals[argno++] = web49_main_expr_to_data(invoke.fun_args[i]);
                    }
                    const char *str = web49_readwat_sym_to_str(invoke.fun_args[0]);
                    web49_section_export_t exports = web49_module_get_section(mod, WEB49_SECTION_ID_EXPORT).export_section;
                    for (size_t j = 0; j < exports.num_entries; j++) {
                        web49_section_export_entry_t entry = exports.entries[j];
                        if (!strcmp(entry.field_str, str)) {
                            web49_interp_data_t data = web49_interp_block_run(interp, &interp.funcs[j]);
                            if (!strcmp(wants.fun_fun, "i32.const")) {
                                int32_t expected = (int32_t) web49_readwat_expr_to_i64(wants.fun_args[0]);
                                if (data.i32_s == expected) {
                                    fprintf(stderr, "wasm spec test: PASS!\n");
                                } else {
                                    fprintf(stderr, "wasm spec test: failed because (actual return value) 0x%"PRIx32" != 0x%"PRIx32" (expected return value)\n", data.i32_u, expected);
                                    return 1;
                                }
                            } else {
                                fprintf(stderr, "wasm spec test: type %s not impelemented\n", wants.fun_fun);
                            }
                            goto test_found;
                        }
                    }
                    fprintf(stderr, "wasm spec test: cannot find export: %s\n", str);
                    return 1;
                test_found:;
                    continue;
                } else if (!strcmp(todo.fun_fun, "assert_trap")) {
                } else if (!strcmp(todo.fun_fun, "assert_invalid")) {
                } else if (!strcmp(todo.fun_fun, "assert_malformed")) {
                } else {
                    // printf("%zu %zu\n", (size_t) todo.start, (size_t) todo.end);
                    fprintf(stderr, "wasm spec test: dont know how to handle: (%s ...)\n", todo.fun_fun);
                    return 1;
                }
            }
            __builtin_trap();
            // fprintf(stderr, "miniwasm cannot handle \"wasm spec test\" files yet!\n");
            // return 1;
        } else {
            mod = web49_readwat_module(&infile);
        }
    }
    web49_opt_tee_module(&mod);
    web49_opt_tree_module(&mod);
    uint32_t start = 0;
    web49_section_export_t exports = web49_module_get_section(mod, WEB49_SECTION_ID_EXPORT).export_section;
    for (size_t j = 0; j < exports.num_entries; j++) {
        web49_section_export_entry_t entry = exports.entries[j];
        if (!strcmp(entry.field_str, "_start")) {
            start = entry.index;
        }
    }
    web49_interp_t interp = web49_interp_module(mod, args);
    interp.import_func = web49_main_import_func;
    interp.import_state = NULL;
    web49_interp_block_run(interp, &interp.funcs[start]);
    web49_free_interp(interp);
    web49_free_module(mod);
    return 0;
}

int main(int argc, const char **argv) {
    const char *inarg = NULL;
    const char **args = argv + 1;
    for (int i = 1; i <= argc; i += 1) {
        if (inarg == NULL) {
            inarg = argv[i];
            args = &argv[i];
        } else {
            break;
        }
    }
    return web49_file_main(inarg, args);
}
