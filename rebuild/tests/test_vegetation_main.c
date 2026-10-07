#include <stdio.h>
int pe_test_vegetation(void);
int pe_test_vegetation_save(void);
int main(void){
    int failures=pe_test_vegetation()+pe_test_vegetation_save();
    printf("Vegetation: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
