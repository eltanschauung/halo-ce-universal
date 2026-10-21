#include "harness.h"
#include "halo_custom_models.h"
#include "jms.inc"
int main(int argc,char **argv)
{
    struct halo_custom_mesh mesh={0};
    struct halo_custom_vertex *expected;
    unsigned short *materials;
    int i,j,count=12,triangles=8,before;
    char const *case_name=argc>1 ? argv[1] : "";
    if(!strcmp(case_name,"large")){count=HALO_CUSTOM_VERTICES;triangles=HALO_CUSTOM_TRIANGLES;}
    mesh.vertex_count=count;mesh.triangle_count=triangles;
    mesh.vertices=calloc(count,sizeof(*mesh.vertices));
    mesh.triangles=malloc(triangles*3*sizeof(*mesh.triangles));
    mesh.triangle_materials=calloc(triangles,sizeof(*mesh.triangle_materials));
    CHECK(mesh.vertices && mesh.triangles && mesh.triangle_materials,"fixture allocations");
    for(i=0;i<count;i++)
    {
        struct halo_custom_vertex *v=&mesh.vertices[i];
        v->position[0]=(float)(i%3);v->normal[2]=1;v->node[1]=-1;
    }
    if(!strcmp(case_name,"seams"))
    {
        mesh.vertices[3].normal[2]=-1;
        mesh.vertices[4].uv[0]=.5f;
        mesh.vertices[5].node[0]=1;
        mesh.vertices[6].node[1]=1;mesh.vertices[6].weight=.25f;
        mesh.vertices[7].weight=.5f;mesh.vertices[7].node[1]=1;
    }
    for(i=0;i<triangles*3;i++)mesh.triangles[i]=(unsigned int)(i%count);
    for(i=0;i<triangles;i++)mesh.triangle_materials[i]=(unsigned short)(i%2);
    expected=malloc((size_t)triangles*3*sizeof(*expected));materials=malloc(triangles*sizeof(*materials));
    CHECK(expected && materials,"snapshot allocation");
    for(i=0;i<triangles*3;i++)expected[i]=mesh.vertices[mesh.triangles[i]];
    memcpy(materials,mesh.triangle_materials,triangles*sizeof(*materials));
    if(!strcmp(case_name,"invalid"))
    {
        mesh.triangles[0]=count;
        CHECK(!halo_custom_mesh_deduplicate(&mesh) && mesh.vertex_count==count,"bad index rejected before mutation");
    }
    else
    {
        CHECK(halo_custom_mesh_deduplicate(&mesh),"deduplicate");
        CHECK(mesh.vertex_count==(!strcmp(case_name,"seams") ? 8:3),"expected unique vertices");
        CHECK(mesh.triangle_count==triangles && !memcmp(materials,mesh.triangle_materials,triangles*sizeof(*materials)),"materials/order retained");
        for(i=0;i<triangles*3;i++)
        {
            CHECK(mesh.triangles[i]<(unsigned int)mesh.vertex_count,"remapped index");
            CHECK(!memcmp(&expected[i],&mesh.vertices[mesh.triangles[i]],sizeof(*expected)),"all corner attributes retained");
        }
        before=mesh.vertex_count;
        CHECK(halo_custom_mesh_deduplicate(&mesh) && mesh.vertex_count==before,"idempotent");
        for(i=0;i<triangles*3;i++)CHECK(!memcmp(&expected[i],&mesh.vertices[mesh.triangles[i]],sizeof(*expected)),"second pass retained data");
        for(j=0;j<2;j++)
        {
            struct halo_custom_mesh part;
            mesh.material_count=2;
            CHECK(halo_custom_mesh_material(&mesh,j,&part),"material remap after deduplication");
            CHECK(part.triangle_count==triangles/2,"material triangle count");
            halo_custom_mesh_dispose(&part);
        }
    }
    free(expected);free(materials);halo_custom_mesh_dispose(&mesh);return 0;
}
