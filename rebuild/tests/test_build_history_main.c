#include <stdio.h>
int pe_test_build_history(void);
int pe_test_build_history_save(void);
int main(void){
    int failures=pe_test_build_history()+pe_test_build_history_save();
    printf("Construction history: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
