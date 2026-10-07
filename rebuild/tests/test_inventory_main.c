#include <stdio.h>
int pe_test_inventory(void);
int pe_test_inventory_save(void);
int pe_test_harvest(void);
int main(void){int n=pe_test_inventory()+pe_test_inventory_save()+pe_test_harvest();printf("Inventory and harvest: %s (%d failures)\n",n?"FAIL":"PASS",n);return n?1:0;}
