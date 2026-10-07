#include <stdio.h>
int pe_test_sword_root_motion(void);
int pe_test_sword_walking(void);
int main(void){
    int failures=pe_test_sword_root_motion()+pe_test_sword_walking();
    printf("Sword walking integration: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
