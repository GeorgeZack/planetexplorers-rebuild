#include <stdio.h>
int pe_test_town_trade(void);
int pe_test_town_trade_save(void);
int main(void){
    int failures=pe_test_town_trade()+pe_test_town_trade_save();
    printf("Town trading: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
