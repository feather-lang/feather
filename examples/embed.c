/**
 * Example: Embedding Feather in a C program
 *
 * Build:
 *   cd /path/to/feather
 *   go build -buildmode=c-shared -o libfeather.so ./cmd/libfeather
 *   cc -o examples/embed examples/embed.c -L. -lfeather -Wl,-rpath,.
 *
 * Run:
 *   ./examples/embed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libfeather.h"

/* Convenience typedefs for cleaner code */
typedef size_t FeatherInterp;
typedef size_t FeatherObj;

#define FEATHER_OK 0

/* Custom command: greet NAME -> "Hello, NAME!" */
static int cmd_greet(void *userData, int argc, char **argv,
                     char **result, char **error) {
    (void)userData;

    if (argc < 1) {
        *error = strdup("wrong # args: should be \"greet name\"");
        return 1;
    }

    /* Build greeting string */
    size_t len = strlen("Hello, ") + strlen(argv[0]) + strlen("!") + 1;
    char *buf = malloc(len);
    snprintf(buf, len, "Hello, %s!", argv[0]);

    *result = buf;
    return 0;
}

/* Foreign type: Counter */
typedef struct {
    int value;
} Counter;

static void* counter_new(void *userData) {
    (void)userData;
    Counter *c = malloc(sizeof(Counter));
    c->value = 0;
    return c;
}

static int counter_invoke(void *instance, const char *method,
                          int argc, char **argv,
                          char **result, char **error) {
    Counter *c = (Counter*)instance;
    char buf[32];

    if (strcmp(method, "get") == 0) {
        snprintf(buf, sizeof(buf), "%d", c->value);
        *result = strdup(buf);
        return 0;
    }

    if (strcmp(method, "set") == 0) {
        if (argc < 1) {
            *error = strdup("wrong # args: should be \"counter set value\"");
            return 1;
        }
        c->value = atoi(argv[0]);
        *result = strdup("");
        return 0;
    }

    if (strcmp(method, "incr") == 0) {
        c->value++;
        snprintf(buf, sizeof(buf), "%d", c->value);
        *result = strdup(buf);
        return 0;
    }

    if (strcmp(method, "add") == 0) {
        if (argc < 1) {
            *error = strdup("wrong # args: should be \"counter add amount\"");
            return 1;
        }
        c->value += atoi(argv[0]);
        snprintf(buf, sizeof(buf), "%d", c->value);
        *result = strdup(buf);
        return 0;
    }

    snprintf(buf, sizeof(buf), "unknown method \"%s\"", method);
    *error = strdup(buf);
    return 1;
}

static void counter_destroy(void *instance) {
    free(instance);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("=== Feather Embedding Example ===\n\n");

    /* Create interpreter */
    FeatherInterp interp = FeatherNew();
    if (!interp) {
        fprintf(stderr, "Failed to create interpreter\n");
        return 1;
    }

    /* Register custom command */
    FeatherRegisterCommand(interp, "greet", cmd_greet, NULL);

    /* Register foreign type */
    FeatherRegisterForeign(interp, "Counter", counter_new, counter_invoke,
                           counter_destroy, NULL);

    /* Example 1: Basic evaluation */
    printf("1. Basic evaluation:\n");
    FeatherObj result;
    if (FeatherEval(interp, "expr 2 + 2", 10, &result) == FEATHER_OK) {
        size_t len;
        char *s = FeatherGetString(result, interp, &len);
        printf("   expr 2 + 2 = %s\n", s);
        FeatherFreeString(s);
    }

    /* Example 2: Custom command */
    printf("\n2. Custom command:\n");
    if (FeatherEval(interp, "greet World", 11, &result) == FEATHER_OK) {
        size_t len;
        char *s = FeatherGetString(result, interp, &len);
        printf("   greet World = %s\n", s);
        FeatherFreeString(s);
    }

    /* Example 3: Variables */
    printf("\n3. Variables:\n");
    FeatherObj name = FeatherString(interp, "Alice", 5);
    FeatherSetVar(interp, "name", name);

    if (FeatherEval(interp, "greet $name", 11, &result) == FEATHER_OK) {
        size_t len;
        char *s = FeatherGetString(result, interp, &len);
        printf("   greet $name = %s\n", s);
        FeatherFreeString(s);
    }

    /* Example 4: Foreign type */
    printf("\n4. Foreign type (Counter):\n");
    const char *counter_script =
        "set c [Counter new]\n"
        "$c set 10\n"
        "$c incr\n"
        "$c add 5\n"
        "$c get";

    if (FeatherEval(interp, (char*)counter_script, strlen(counter_script), &result) == FEATHER_OK) {
        size_t len;
        char *s = FeatherGetString(result, interp, &len);
        printf("   Counter: 10 -> incr -> add 5 = %s\n", s);
        FeatherFreeString(s);
    }

    /* Example 5: List operations */
    printf("\n5. List operations:\n");
    FeatherObj items[3];
    items[0] = FeatherString(interp, "apple", 5);
    items[1] = FeatherString(interp, "banana", 6);
    items[2] = FeatherString(interp, "cherry", 6);

    FeatherObj list = FeatherList(interp, 3, items);
    printf("   List length: %zu\n", FeatherListLength(list, interp));

    FeatherObj elem = FeatherListAt(interp, list, 1);
    if (elem) {
        size_t len;
        char *s = FeatherGetString(elem, interp, &len);
        printf("   Element at index 1: %s\n", s);
        FeatherFreeString(s);
    }

    /* Example 6: Dict operations */
    printf("\n6. Dict operations:\n");
    FeatherObj dict = FeatherDict(interp);

    FeatherObj key = FeatherString(interp, "name", 4);
    FeatherObj val = FeatherString(interp, "Bob", 3);
    dict = FeatherDictSet(interp, dict, key, val);

    key = FeatherString(interp, "age", 3);
    val = FeatherInt(interp, 30);
    dict = FeatherDictSet(interp, dict, key, val);

    printf("   Dict size: %zu\n", FeatherDictSize(dict, interp));

    FeatherObj keys = FeatherDictKeys(interp, dict);
    size_t len;
    char *s = FeatherGetString(keys, interp, &len);
    printf("   Dict keys: %s\n", s);
    FeatherFreeString(s);

    /* Clean up */
    FeatherClose(interp);

    printf("\n=== Done ===\n");
    return 0;
}
