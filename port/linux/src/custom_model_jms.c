/* Strict, bounded JMS 8200 reader for the Chief skeleton and named materials. These are authoring inputs, never packets or executable tags. */
#include "halo_custom_models.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
struct reader { char const *at, *end; int failed; };
static void space(struct reader *r) { while(r->at<r->end && (*r->at==' ' || *r->at=='\t' || *r->at=='\r' || *r->at=='\n')) r->at++; }
static double number(struct reader *r)
{
    char token[64],*end; size_t n=0; double v;
    space(r);
    while(r->at<r->end && *r->at!=' ' && *r->at!='\t' && *r->at!='\r' && *r->at!='\n')
    { if(n+1<sizeof(token)) token[n++]=*r->at; else r->failed=1; r->at++; }
    token[n]=0; v=strtod(token,&end);
    if (!n || *end || !isfinite(v) || v < -2147483648.0 || v > 4294967295.0) {r->failed=1;return 0;}
    return v;
}
static int integer(struct reader *r, int low, int high)
{
    double v=number(r);
    if (v<low || v>high || v!=(int)v) {r->failed=1;return low;}
    return (int)v;
}
static float real_value(struct reader *r, float low, float high)
{ double v=number(r);if(v<low || v>high) {r->failed=1;return 0;}return (float)v; }
static void line(struct reader *r, char *text, size_t capacity)
{
    size_t n=0;
    space(r);
    while(r->at<r->end && *r->at!='\r' && *r->at!='\n')
    {if(n+1<capacity)text[n++]=*r->at;else r->failed=1;r->at++;}
    while(n && (text[n-1]==' ' || text[n-1]=='\t')) n--;
    text[n]=0;
    if (!n) r->failed=1;
}
/* Only local texture leaves are resolved; authoring-machine directories are
   never opened. Material labels stay separate from their bitmap hints. */
int halo_custom_texture_name_valid(char const *name)
{
    size_t i,n=name ? strlen(name) : 0;
    if(!n || n>=96 || strstr(name,"..") || name[0]=='.')return 0;
    for(i=0;i<n;i++)if(!((name[i]>='a' && name[i]<='z') || (name[i]>='A' && name[i]<='Z') ||
        (name[i]>='0' && name[i]<='9') || name[i]=='_' || name[i]=='-' || name[i]=='.' || name[i]==' '))return 0;
    return name[n-1]!='.' && name[n-1]!=' ';
}
static void material_texture(char const *name,char const *hint,char texture[96])
{
    char const *leaf=hint,*p;
    size_t n;
    for(p=hint;*p;p++)if(*p=='/' || *p=='\\')leaf=p+1;
    /* Preserve the original armor.png convention despite stale JMS hints. */
    if(!strcmp(name,"armor") || !halo_custom_texture_name_valid(leaf))leaf=name;
    strcpy(texture,leaf);n=strlen(texture);
    if(n>4 && (!strcmp(texture+n-4,".png") || !strcmp(texture+n-4,".bmp") ||
        !strcmp(texture+n-4,".PNG") || !strcmp(texture+n-4,".BMP")))texture[n-4]=0;
}
void halo_custom_mesh_dispose(struct halo_custom_mesh *m)
{free(m->vertices);free(m->triangles);free(m->triangle_materials);memset(m,0,sizeof(*m));}
int halo_custom_mesh_parse(char const *data, unsigned int size, struct halo_custom_mesh *m)
{
    struct reader r;
    int i,j,regions,parents[HALO_CUSTOM_MAX_NODES]; char name[512];
    memset(m,0,sizeof(*m));
    if (!data || !size || size>HALO_CUSTOM_JMS_BYTES || memchr(data,0,size)) return 0;
    r.at=data;r.end=data+size;r.failed=0;
    if(integer(&r,8200,8200)!=8200) return 0;
    number(&r); /* authoring checksum; runtime uses the validated stock nodes */
    m->node_count=integer(&r,1,HALO_CUSTOM_MAX_NODES);if(r.failed)return 0;
    for(i=0;i<m->node_count;i++)
    {
        struct halo_custom_node *node=&m->nodes[i];
        line(&r,node->name,sizeof(node->name));
        node->child=integer(&r,-1,m->node_count-1);node->sibling=integer(&r,-1,m->node_count-1);node->parent=-1;parents[i]=-1;
        {
            float norm=0;
            for(j=0;j<4;j++){node->rotation[j]=real_value(&r,-1.001f,1.001f);norm+=node->rotation[j]*node->rotation[j];}
            if(norm<.999f || norm>1.001f)r.failed=1;
        }
        for(j=0;j<3;j++)node->translation[j]=real_value(&r,-10000.f,10000.f)/100.f;
    }
    for(i=0;i<m->node_count;i++)
    {
        int at=m->nodes[i].child,steps=0;
        while(at!=-1 && steps++<m->node_count)
        {if(at==0 || parents[at]!=-1)r.failed=1;parents[at]=i;at=m->nodes[at].sibling;}
        if(steps>m->node_count)r.failed=1;
    }
    for(i=0;i<m->node_count;i++)
    {
        int at=i,steps=0;
        m->nodes[i].parent=parents[i];
        while(at!=-1 && steps++<=m->node_count)at=parents[at];
        if(steps>m->node_count || (i && parents[i]==-1))r.failed=1;
    }
    if(m->nodes[0].sibling!=-1)r.failed=1;
    m->material_count=integer(&r,1,HALO_CUSTOM_MATERIALS);
    for(i=0;i<m->material_count;i++)
    {
        struct halo_custom_material *material=&m->materials[i];
        line(&r,material->name,sizeof(material->name));line(&r,name,sizeof(name));
        if(!halo_custom_texture_name_valid(material->name))r.failed=1;
        for(j=0;j<i;j++)if(!strcmp(material->name,m->materials[j].name))r.failed=1;
        material_texture(material->name,name,material->texture);
    }
    m->marker_count=integer(&r,0,256);
    for(i=0;i<m->marker_count;i++)
    {
        struct halo_custom_marker *marker=&m->markers[i];float norm=0;
        line(&r,marker->name,sizeof(marker->name));integer(&r,-1,31);marker->node=integer(&r,-1,m->node_count-1);
        for(j=0;j<4;j++){marker->rotation[j]=real_value(&r,-1.001f,1.001f);norm+=marker->rotation[j]*marker->rotation[j];}
        if(norm<.999f || norm>1.001f)r.failed=1;
        for(j=0;j<3;j++)marker->translation[j]=real_value(&r,-10000.f,10000.f)/100.f;
        real_value(&r,0,10000.f);
    }
    regions=integer(&r,1,8);
    for(i=0;i<regions;i++)line(&r,name,sizeof(name));
    m->vertex_count=integer(&r,3,HALO_CUSTOM_VERTICES);
    if(r.failed)goto fail;
    m->vertices=calloc(m->vertex_count,sizeof(*m->vertices));
    if(!m->vertices)goto fail;
    for(i=0;i<m->vertex_count;i++)
    {
        struct halo_custom_vertex *v=&m->vertices[i];
        float length=0;
        v->node[0]=integer(&r,0,m->node_count-1);
        for(j=0;j<3;j++)v->position[j]=real_value(&r,-10000.f,10000.f)/100.f;
        for(j=0;j<3;j++){v->normal[j]=real_value(&r,-1.001f,1.001f);length+=v->normal[j]*v->normal[j];}
        if(length<.9f || length>1.1f)r.failed=1;
        v->node[1]=integer(&r,-1,m->node_count-1);v->weight=real_value(&r,0,1);
        if(v->node[1]==-1 && v->weight!=0)r.failed=1;
        v->uv[0]=real_value(&r,-16,16);v->uv[1]=1.f-real_value(&r,-16,16);real_value(&r,-16,16);
    }
    m->triangle_count=integer(&r,1,HALO_CUSTOM_TRIANGLES);
    if(r.failed)goto fail;
    m->triangles=malloc((size_t)m->triangle_count*3*sizeof(*m->triangles));
    m->triangle_materials=malloc((size_t)m->triangle_count*sizeof(*m->triangle_materials));
    if(!m->triangles || !m->triangle_materials)goto fail;
    for(i=0;i<m->triangle_count;i++)
    {
        integer(&r,0,regions-1);m->triangle_materials[i]=(unsigned short)integer(&r,0,m->material_count-1);
        /* port: JMS faces wind opposite native model buffers. Reverse the
           face, preserving its authored normals and per-vertex attributes. */
        m->triangles[i*3]=(unsigned int)integer(&r,0,m->vertex_count-1);
        m->triangles[i*3+2]=(unsigned int)integer(&r,0,m->vertex_count-1);
        m->triangles[i*3+1]=(unsigned int)integer(&r,0,m->vertex_count-1);
    }
    space(&r);
    if(r.failed || r.at!=r.end)goto fail;
    return 1;
fail:halo_custom_mesh_dispose(m);return 0;
}

/* Collapse only identical parsed records. Normal/UV/skin discontinuities stay
   separate, including the opposite-facing vertices of an outline shell. */
int halo_custom_mesh_deduplicate(struct halo_custom_mesh *mesh)
{
    unsigned int *table,*remap,capacity=1;
    struct halo_custom_vertex *smaller;
    int i,j,count=0;
    if(!mesh || mesh->vertex_count<1 || mesh->vertex_count>HALO_CUSTOM_VERTICES ||
       !mesh->vertices || mesh->triangle_count<1 || mesh->triangle_count>HALO_CUSTOM_TRIANGLES || !mesh->triangles)return 0;
    for(i=0;i<mesh->triangle_count*3;i++)if(mesh->triangles[i]>=(unsigned int)mesh->vertex_count)return 0;
    while(capacity<(unsigned int)mesh->vertex_count*2)capacity*=2;
    table=calloc(capacity,sizeof(*table));
    remap=malloc((size_t)mesh->vertex_count*sizeof(*remap));
    if(!table || !remap){free(table);free(remap);return 0;}
    for(i=0;i<mesh->vertex_count;i++)
    {
        struct halo_custom_vertex const *vertex=&mesh->vertices[i];
        unsigned char const *bytes=(unsigned char const *)vertex;
        unsigned int hash=2166136261u,slot;
        for(j=0;j<(int)sizeof(*vertex);j++)hash=(hash^bytes[j])*16777619u;
        slot=hash&(capacity-1);
        while(table[slot] && memcmp(vertex,&mesh->vertices[table[slot]-1],sizeof(*vertex)))slot=(slot+1)&(capacity-1);
        if(!table[slot])
        {
            if(count!=i)mesh->vertices[count]=*vertex;
            table[slot]=(unsigned int)++count;
        }
        remap[i]=table[slot]-1;
    }
    for(i=0;i<mesh->triangle_count*3;i++)mesh->triangles[i]=remap[mesh->triangles[i]];
    mesh->vertex_count=count;
    smaller=realloc(mesh->vertices,(size_t)count*sizeof(*smaller));
    if(smaller)mesh->vertices=smaller;
    free(remap);free(table);return 1;
}

/* Compact each material independently, then split large materials into
   native 16-bit draw buffers without changing their shader or triangles. */
int halo_custom_mesh_material(struct halo_custom_mesh const *mesh,int material,struct halo_custom_mesh *part)
{
    int *remap;
    int i,j,count=0;
    memset(part,0,sizeof(*part));
    if(material<0 || material>=mesh->material_count)return 0;
    for(i=0;i<mesh->triangle_count;i++)if(mesh->triangle_materials[i]==material)count++;
    if(!count)return 1;
    remap=malloc((size_t)mesh->vertex_count*sizeof(*remap));
    part->vertices=malloc((size_t)(mesh->vertex_count<HALO_CUSTOM_MATERIAL_VERTICES ? mesh->vertex_count : HALO_CUSTOM_MATERIAL_VERTICES)*sizeof(*part->vertices));
    part->triangles=malloc((size_t)count*3*sizeof(*part->triangles));
    if(!remap || !part->vertices || !part->triangles)goto fail;
    for(i=0;i<mesh->vertex_count;i++)remap[i]=-1;
    for(i=0;i<mesh->triangle_count;i++)if(mesh->triangle_materials[i]==material)
    {
        for(j=0;j<3;j++)
        {
            unsigned int old=mesh->triangles[i*3+j];
            if(old>=(unsigned int)mesh->vertex_count)goto fail;
            if(remap[old]<0)
            {
                if(part->vertex_count==HALO_CUSTOM_MATERIAL_VERTICES)goto fail;
                remap[old]=part->vertex_count;part->vertices[part->vertex_count++]=mesh->vertices[old];
            }
            part->triangles[part->triangle_count*3+j]=(unsigned int)remap[old];
        }
        part->triangle_count++;
    }
    free(remap);return 1;
fail:free(remap);halo_custom_mesh_dispose(part);return 0;
}

/* At most three distinct vertices per triangle: this bound guarantees that
   a chunk fits native unsigned-short indices even with fully split JMS data. */
int halo_custom_mesh_chunk(struct halo_custom_mesh const *mesh,int first,int count,struct halo_custom_mesh *part)
{
    int *remap,i,j,old;
    memset(part,0,sizeof(*part));
    if(first<0 || count<1 || count>HALO_CUSTOM_CHUNK_TRIANGLES ||
       first>mesh->triangle_count || count>mesh->triangle_count-first)return 0;
    remap=malloc((size_t)mesh->vertex_count*sizeof(*remap));
    part->vertices=malloc((size_t)count*3*sizeof(*part->vertices));
    part->triangles=malloc((size_t)count*3*sizeof(*part->triangles));
    if(!remap || !part->vertices || !part->triangles)goto fail;
    for(i=0;i<mesh->vertex_count;i++)remap[i]=-1;
    for(i=0;i<count;i++)for(j=0;j<3;j++)
    {
        unsigned int index=mesh->triangles[(first+i)*3+j];
        if(index>=(unsigned int)mesh->vertex_count)goto fail;
        old=(int)index;
        if(remap[old]<0)
        {remap[old]=part->vertex_count;part->vertices[part->vertex_count++]=mesh->vertices[old];}
        part->triangles[i*3+j]=(unsigned int)remap[old];
    }
    part->triangle_count=count;free(remap);return 1;
fail:free(remap);halo_custom_mesh_dispose(part);return 0;
}
