/*
 * testlocale.c
 *
 */


#include <locale.h>
#include <stdio.h>

int main() {
    int i;
    struct lconv* current_conv;
    printf("LC_ALL: %d\n", LC_ALL);
    printf("LC_TIME: %d\n", LC_TIME);
    printf("LC_NUMERIC: %d\n", LC_NUMERIC);
    printf("LC_MONETARY: %d\n", LC_MONETARY);
    printf("Current locale: %s\n", setlocale(LC_ALL, (char*) 0));
    setlocale(LC_ALL, "C");
    current_conv = localeconv();
    printf("LC_NUMERIC: \n---------------------\n");
    printf("Decimal point: %s\n", current_conv->decimal_point);
    printf("Thousands separator: %s\n", current_conv->thousands_sep);
    printf("Grouping: \n");
    for (i = 0; i < 1024; i++) {
        printf("%d ", current_conv->grouping[i]);
        if (0 == current_conv->grouping[i]) {
            printf("\n");
            break;
        }
    }
    printf("LC_MONETARY\n---------------------\n");
    printf("Int. currency symbol: %s\n", current_conv->int_curr_symbol);
    printf("Currency symbol: %s\n", current_conv->currency_symbol);
    printf("Monetary decimal point: %s\n", current_conv->mon_decimal_point);
    printf("Monetary thousands separator: %s\n", current_conv->mon_thousands_sep);
    printf("Monetary grouping: \n");
    for (i = 0; i < 1024; i++) {
        printf("%d ", current_conv->mon_grouping[i]);
        if (0 == current_conv->mon_grouping[i]) {
            printf("\n");
            break;
        }
    }
    printf("Positive sign: %s\n", current_conv->positive_sign);
    printf("Negative sign: %s\n", current_conv->negative_sign);
    printf("Int. fractional digits: %d\n", current_conv->int_frac_digits);
    printf("Currency symbol precedes positive value: %d\n", current_conv->p_cs_precedes);
    printf("Currency symbol precedes positive value (Int.): %d\n", current_conv->int_p_cs_precedes);
    printf("Currency symbol separated by space: %d\n", current_conv->p_sep_by_space);
    printf("Currency symbol separated by space (Int.): %d\n", current_conv->int_p_sep_by_space);
    printf("Positive sign position: %d\n", current_conv->p_sign_posn);
    printf("Negative sign position: %d\n", current_conv->n_sign_posn);
    printf("Positive sign position (Int.): %d\n", current_conv->int_p_sign_posn);
    printf("Negative sign position (Int.): %d\n", current_conv->int_n_sign_posn);
    return 0;
}
