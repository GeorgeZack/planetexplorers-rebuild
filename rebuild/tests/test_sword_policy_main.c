/* Focused source branch, shared request and recovered collision/contact suite.
 * No graphics/desktop input or arranged hunt-completion claim. */
#include <stdio.h>
int pe_test_sword_variant(void);
int pe_test_sword_motion(void);
int pe_test_running_attack_motion(void);
int pe_test_sandbox_melee(void);
int pe_test_running_attack_pipeline(void);
int main(void){
    int errors=pe_test_sword_variant()+pe_test_sword_motion()+pe_test_running_attack_motion()+
        pe_test_sandbox_melee()+pe_test_running_attack_pipeline();
    printf("Sword policy integration: %s (%d failures)\n",errors?"FAIL":"PASS",errors);
    return errors?1:0;
}
