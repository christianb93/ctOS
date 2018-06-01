/*
 * testlocale.c
 *
 */


#include <locale.h>
#include <stdio.h>
#include <langinfo.h>

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
    printf("nl_langinfo\n---------------------\n");
    printf("CODESET:              %s\n", nl_langinfo(CODESET));
    printf("D_T_FMT:              %s\n", nl_langinfo(D_T_FMT));
    printf("D_FMT:                %s\n", nl_langinfo(D_FMT));
    printf("T_FMT:                %s\n", nl_langinfo(T_FMT));
    printf("T_FMT_AMPM:           %s\n", nl_langinfo(T_FMT_AMPM));
    printf("AM_STR:               %s\n", nl_langinfo(AM_STR));
    printf("PM_STR:               %s\n", nl_langinfo(PM_STR));
    printf("DAY_1   (%d):           %s\n", DAY_1, nl_langinfo(DAY_1));
    printf("DAY_2   (%d):           %s\n", DAY_2, nl_langinfo(DAY_2));
    printf("DAY_3   (%d):           %s\n", DAY_3, nl_langinfo(DAY_3));
    printf("DAY_4   (%d):           %s\n", DAY_4, nl_langinfo(DAY_4));
    printf("DAY_5   (%d):           %s\n", DAY_5, nl_langinfo(DAY_5));
    printf("DAY_6   (%d):           %s\n", DAY_6, nl_langinfo(DAY_6));
    printf("DAY_7   (%d):           %s\n", DAY_7, nl_langinfo(DAY_7));
    printf("ABDAY_1 (%d):           %s\n", ABDAY_1, nl_langinfo(ABDAY_1));
    printf("ABDAY_2 (%d):           %s\n", ABDAY_2, nl_langinfo(ABDAY_2));
    printf("ABDAY_3 (%d):           %s\n", ABDAY_3, nl_langinfo(ABDAY_3));
    printf("ABDAY_4 (%d):           %s\n", ABDAY_4, nl_langinfo(ABDAY_4));
    printf("ABDAY_5 (%d):           %s\n", ABDAY_5, nl_langinfo(ABDAY_5));
    printf("ABDAY_6 (%d):           %s\n", ABDAY_6, nl_langinfo(ABDAY_6));
    printf("ABDAY_7 (%d):           %s\n", ABDAY_7, nl_langinfo(ABDAY_7));

    printf("MON_1   (%d):           %s\n", MON_1, nl_langinfo(MON_1));
    printf("MON_2   (%d):           %s\n", MON_2, nl_langinfo(MON_2));
    printf("MON_3   (%d):           %s\n", MON_3, nl_langinfo(MON_3));
    printf("MON_4   (%d):           %s\n", MON_4, nl_langinfo(MON_4));
    printf("MON_5   (%d):           %s\n", MON_5, nl_langinfo(MON_5));
    printf("MON_6   (%d):           %s\n", MON_6, nl_langinfo(MON_6));
    printf("MON_7   (%d):           %s\n", MON_7, nl_langinfo(MON_7));
    printf("MON_8   (%d):           %s\n", MON_8, nl_langinfo(MON_8));
    printf("MON_9   (%d):           %s\n", MON_9, nl_langinfo(MON_9));
    printf("MON_10  (%d):           %s\n", MON_10, nl_langinfo(MON_10));
    printf("MON_11  (%d):           %s\n", MON_11, nl_langinfo(MON_11));
    printf("MON_12  (%d):           %s\n", MON_12, nl_langinfo(MON_12));


    printf("ABMON_1 (%d):           %s\n", ABMON_1, nl_langinfo(ABMON_1));
    printf("ABMON_2 (%d):           %s\n", ABMON_2, nl_langinfo(ABMON_2));
    printf("ABMON_3 (%d):           %s\n", ABMON_3, nl_langinfo(ABMON_3));
    printf("ABMON_4 (%d):           %s\n", ABMON_4, nl_langinfo(ABMON_4));
    printf("ABMON_5 (%d):           %s\n", ABMON_5, nl_langinfo(ABMON_5));
    printf("ABMON_6 (%d):           %s\n", ABMON_6, nl_langinfo(ABMON_6));
    printf("ABMON_7 (%d):           %s\n", ABMON_7, nl_langinfo(ABMON_7));
    printf("ABMON_8 (%d):           %s\n", ABMON_8, nl_langinfo(ABMON_8));
    printf("ABMON_9 (%d):           %s\n", ABMON_9, nl_langinfo(ABMON_9));
    printf("ABMON_10(%d):           %s\n", ABMON_10, nl_langinfo(ABMON_10));
    printf("ABMON_11(%d):           %s\n", ABMON_11, nl_langinfo(ABMON_11));
    printf("ABMON_12(%d):           %s\n", ABMON_12, nl_langinfo(ABMON_12));
    
    printf("ERA:                    %s\n", nl_langinfo(ERA));
    printf("ERA_D_FMT:              %s\n", nl_langinfo(ERA_D_FMT));
    printf("ERA_D_T_FMT:            %s\n", nl_langinfo(ERA_D_T_FMT));
    printf("ERA_T_FMT:              %s\n", nl_langinfo(ERA_D_T_FMT));
    
    printf("ALT_DIGITS:             %s\n", nl_langinfo(ALT_DIGITS));
    printf("RADIXCHAR:              %s\n", nl_langinfo(RADIXCHAR));
    printf("THOUSEP:                %s\n", nl_langinfo(THOUSEP));
    printf("YESEXPR:                %s\n", nl_langinfo(YESEXPR));
    printf("NOEXPR:                 %s\n", nl_langinfo(NOEXPR));
    printf("CRNCYSTR:               %s\n", nl_langinfo(CRNCYSTR));
    

    return 0;
}
