#include "../display_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    assert(ngb_output_image_count(2,0,1)==3);
    assert(ngb_output_image_count(2,3,1)==3);
    assert(ngb_output_image_count(2,2,1)==2);
    assert(ngb_output_image_count(2,1,1)==1);
    assert(ngb_output_image_count(4,8,1)==4);
    assert(ngb_output_image_count(4,3,1)==3);
    assert(ngb_output_image_count(2,0,0)==2);
    assert(ngb_output_image_count(4,3,0)==3);
    puts("iOS drawable policy: spare image, driver limits and standalone Mac behavior passed");return 0;
}
