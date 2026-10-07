#include <stdio.h>
int pe_test_town_wallet(void);
int pe_test_town_wallet_save(void);
int main(void){
    int failures=pe_test_town_wallet()+pe_test_town_wallet_save();
    printf("Town wallets: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
