#include "harness.h"
#include "halo_custom_models.h"
struct bitmap_data {void *base_address,*hardware_format;};
static struct bitmap_data bitmap;
static unsigned int pixels[4];
static int failure,deleted,disposed;
static struct bitmap_data *bitmap_2d_new(short w,short h,short m,short f)
{CHECK(w==2 && h==2 && m==0 && f==11,"bitmap dimensions/format");bitmap.base_address=failure==2 ? NULL:pixels;bitmap.hardware_format=NULL;return failure==1 ? NULL:&bitmap;}
static void bitmap_rebuild(struct bitmap_data *b){CHECK(b==&bitmap,"bitmap identity");if(failure!=3)b->hardware_format=(void*)1;}
static void bitmap_delete(struct bitmap_data *b){CHECK(b==&bitmap,"delete identity");deleted++;}
void halo_custom_image_dispose(struct halo_custom_image *image){free(image->pixels);image->pixels=NULL;disposed++;}
#include "bitmap.inc"
int main(int argc,char **argv)
{
    struct halo_custom_image image={2,2,NULL};struct bitmap_data *result;
    CHECK(argc==2,"case");failure=atoi(argv[1]);image.pixels=malloc(4*sizeof(*image.pixels));CHECK(image.pixels,"fixture");
    image.pixels[0]=0xFF123456u;image.pixels[1]=1;image.pixels[2]=2;image.pixels[3]=3;
    result=bitmap_from_image(&image);
    CHECK(disposed==1 && !image.pixels,"decoded image released");
    CHECK((result!=NULL)==(failure==0),"only an uploaded texture is usable");
    CHECK(deleted==((failure==2 || failure==3) ? 1:0),"failed bitmap cleaned once");
    if(!failure)CHECK(pixels[0]==0xFF123456u && pixels[3]==3,"image data preserved");
    return 0;
}
