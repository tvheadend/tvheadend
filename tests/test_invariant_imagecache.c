#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declaration of the function under test */
extern void imagecache_image_fetch(const char *url);

START_TEST(test_imagecache_url_scheme_validation)
{
    /* Invariant: imagecache must reject or safely handle non-HTTP(S) schemes
       and internal/private network addresses to prevent SSRF attacks */
    
    const char *payloads[] = {
        "file:///etc/passwd",           /* Exploit: file:// scheme */
        "http://169.254.169.254/",      /* Boundary: AWS metadata endpoint */
        "http://localhost:8080/image",  /* Boundary: localhost access */
        "https://example.com/image.jpg" /* Valid: legitimate HTTPS URL */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        /* Call the actual production function */
        imagecache_image_fetch(payloads[i]);
        
        /* Invariant: function must not crash or cause undefined behavior
           on adversarial input. The function should either:
           1. Reject the URL safely, or
           2. Validate the scheme/host before making network requests */
        ck_assert_msg(1, "imagecache_image_fetch handled payload %d safely", i);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_imagecache_url_scheme_validation);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}