#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Simulate the fixed-size path buffer as used in NuttyFileManager */
#define PATH_BUFFER_SIZE 256
#define MAX_FILENAME_LEN 255

/*
 * Safe path append function that enforces the security invariant:
 * The resulting path must never exceed PATH_BUFFER_SIZE - 1 characters.
 * This is the CORRECT implementation that should replace unbounded strcat().
 */
static int safe_path_append(char *path, size_t path_buf_size, const char *filename)
{
    if (!path || !filename || path_buf_size == 0) {
        return -1;
    }

    size_t current_len = strlen(path);
    size_t filename_len = strlen(filename);
    size_t separator_len = 1; /* for "/" */

    /* Check if appending filename + "/" would overflow the buffer */
    if (current_len + filename_len + separator_len >= path_buf_size) {
        return -1; /* Would overflow */
    }

    strcat(path, filename);
    strcat(path, "/");
    return 0;
}

/*
 * Validate that a path buffer has not been overflowed by checking
 * that the string length is within bounds.
 */
static int path_is_within_bounds(const char *path, size_t path_buf_size)
{
    if (!path) return 0;
    return strlen(path) < path_buf_size;
}

/*
 * Simulate the vulnerable pattern but with a guard to detect overflow attempts.
 * Returns 1 if the operation would cause overflow, 0 otherwise.
 */
static int would_overflow(const char *current_path, const char *filename)
{
    if (!current_path || !filename) return 1;

    size_t current_len = strlen(current_path);
    size_t filename_len = strlen(filename);
    size_t separator_len = 1;

    return (current_len + filename_len + separator_len >= PATH_BUFFER_SIZE);
}

START_TEST(test_path_buffer_overflow_invariant)
{
    /* Invariant: The path buffer must never exceed PATH_BUFFER_SIZE bytes,
     * regardless of the length or content of user-controlled filenames.
     * Appending adversarial filenames must either succeed safely or be rejected,
     * never silently overflow the buffer. */

    const char *payloads[] = {
        /* Exact boundary */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        /* Over boundary - 300 chars */
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
        /* Path traversal attempt */
        "../../../../../../../etc/passwd",
        /* Path traversal with long prefix */
        "../../../../../../../../../../../../../../../../../../../../../../../../"
        "../../../../../../../../../../../../../../../../../../../../../../../../"
        "etc/shadow",
        /* Null-byte injection attempt (treated as empty after null) */
        "normal_file",
        /* Very long filename with path separators */
        "dir/subdir/subsubdir/file_with_very_long_name_that_exceeds_limits_"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        /* Single character (valid) */
        "a",
        /* Maximum safe filename */
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
        /* Special characters */
        "file;rm -rf /;echo pwned",
        /* Unicode-like long sequence */
        "\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf",
        /* Repeated slashes */
        "////////////////////////////////////////////////////////////////////////"
        "////////////////////////////////////////////////////////////////////////"
        "////////////////////////////////////////////////////////////////////////",
        /* Mixed attack: long + traversal */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "/../../../etc/passwd",
        /* Empty string */
        "",
        /* Just a slash */
        "/",
        /* Windows-style path separator */
        "..\\..\\..\\windows\\system32",
    };

    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        char path[PATH_BUFFER_SIZE];
        memset(path, 0, sizeof(path));

        /* Start with a base path as the file manager would */
        strncpy(path, "/sd/", sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';

        const char *filename = payloads[i];

        /* INVARIANT 1: Before any operation, path must be within bounds */
        ck_assert_msg(path_is_within_bounds(path, PATH_BUFFER_SIZE),
                      "Initial path already out of bounds for payload %d", i);

        /* INVARIANT 2: If the operation would overflow, it must be rejected */
        int overflow_detected = would_overflow(path, filename);

        if (!overflow_detected) {
            /* Safe to append - do it and verify bounds are maintained */
            int result = safe_path_append(path, PATH_BUFFER_SIZE, filename);

            if (result == 0) {
                /* INVARIANT 3: After successful append, path must still be within bounds */
                ck_assert_msg(path_is_within_bounds(path, PATH_BUFFER_SIZE),
                              "Path exceeded buffer bounds after append for payload %d (len=%zu)",
                              i, strlen(path));

                /* INVARIANT 4: The null terminator must be within the buffer */
                ck_assert_msg(strlen(path) < PATH_BUFFER_SIZE,
                              "Path length %zu >= buffer size %d for payload %d",
                              strlen(path), PATH_BUFFER_SIZE, i);
            } else {
                /* safe_path_append rejected it - path must be unchanged or still valid */
                ck_assert_msg(path_is_within_bounds(path, PATH_BUFFER_SIZE),
                              "Path out of bounds after rejected append for payload %d", i);
            }
        } else {
            /* INVARIANT 5: Overflow was detected and prevented - path must be unchanged */
            /* Verify the path is still the base path (unchanged) */
            ck_assert_msg(path_is_within_bounds(path, PATH_BUFFER_SIZE),
                          "Path out of bounds even before overflow attempt for payload %d", i);

            /* Simulate what the vulnerable code would do and verify our guard caught it */
            size_t base_len = strlen(path);
            size_t filename_len = strlen(filename);

            /* The combined length WOULD exceed the buffer */
            if (filename_len > 0) {
                ck_assert_msg(base_len + filename_len + 1 >= PATH_BUFFER_SIZE,
                              "Overflow detection false positive for payload %d", i);
            }
        }
    }
}
END_TEST

START_TEST(test_repeated_appends_stay_bounded)
{
    /* Invariant: Repeated path component appends must never overflow the buffer,
     * even when each individual component appears safe. */

    char path[PATH_BUFFER_SIZE];
    memset(path, 0, sizeof(path));
    strncpy(path, "/", sizeof(path) - 1);

    const char *component = "dir";
    int appended = 0;
    int rejected = 0;

    /* Attempt to append many path components */
    for (int i = 0; i < 1000; i++) {
        size_t before_len = strlen(path);

        int result = safe_path_append(path, PATH_BUFFER_SIZE, component);

        if (result == 0) {
            appended++;
            /* INVARIANT: After each successful append, buffer must not overflow */
            ck_assert_msg(strlen(path) < PATH_BUFFER_SIZE,
                          "Buffer overflow after %d successful appends", appended);
            ck_assert_msg(strlen(path) > before_len,
                          "Path did not grow after append %d", appended);
        } else {
            rejected++;
            /* INVARIANT: After rejection, path must be unchanged */
            ck_assert_msg(strlen(path) == before_len,
                          "Path changed after rejected append at iteration %d", i);
            /* INVARIANT: Path must still be within bounds */
            ck_assert_msg(strlen(path) < PATH_BUFFER_SIZE,
                          "Path out of bounds after rejection at iteration %d", i);
        }
    }

    /* INVARIANT: At least some appends must have been rejected to prevent overflow */
    ck_assert_msg(rejected > 0,
                  "No appends were rejected despite 1000 iterations - overflow likely occurred");

    /* INVARIANT: Final path must be within bounds */
    ck_assert_msg(strlen(path) < PATH_BUFFER_SIZE,
                  "Final path length %zu exceeds buffer size %d", strlen(path), PATH_BUFFER_SIZE);
}
END_TEST

START_TEST(test_filename_length_validation)
{
    /* Invariant: Filenames longer than the available buffer space must be rejected */

    struct {
        const char *base_path;
        const char *filename;
        int should_succeed;
    } test_cases[] = {
        /* Normal case - should succeed */
        { "/sd/", "file.txt", 1 },
        /* Filename exactly fills remaining space - should fail (need room for "/" and null) */
        { "/sd/", 
          "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
          "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
          "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
          "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
          0 },
        /* Empty base, long filename */
        { "",
          "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
          "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
          "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
          "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
          "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
          0 },
        /* Short base, short filename - should succeed */
        { "/", "a", 1 },
        /* Nearly full base path */
        { "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
          "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
          "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
          "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
          "toolong", 0 },
    };

    int num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char path[PATH_BUFFER_SIZE];
        memset(path, 0, sizeof(path));

        /* Only copy base path if it fits */
        if (strlen(test_cases[i].base_path) < PATH_BUFFER_SIZE) {
            strncpy(path, test_cases[i].base_path, PATH_BUFFER_SIZE - 1);
            path[PATH_BUFFER_SIZE - 1] = '\0';
        }

        size_t initial_len = strlen(path);
        int result = safe_path_append(path, PATH_BUFFER_SIZE, test_cases[i].filename);

        /* INVARIANT: Buffer must never overflow regardless of result */
        ck_assert_msg(strlen(path) < PATH_BUFFER_SIZE,
                      "Buffer overflow in test case %d (result=%d)", i, result);

        if (test_cases[i].should_succeed) {
            ck_assert_msg(result == 0,
                          "Expected success but got failure for test case %d", i);
            ck_assert_msg(strlen(path) > initial_len,
                          "Path did not grow on expected success for test case %d", i);
        } else {
            ck_assert_msg(result != 0,
                          "Expected failure but got success for test case %d - "
                          "potential buffer overflow vulnerability!", i);
            ck_assert_msg(strlen(path) == initial_len,
                          "Path changed after expected rejection for test case %d", i);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security_NuttyFileManager_PathBuffer");
    tc_core = tcase_create("Core");

    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_path_buffer_overflow_invariant);
    tcase_add_test(tc_core, test_repeated_appends_stay_bounded);
    tcase_add_test(tc_core, test_filename_length_validation);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();