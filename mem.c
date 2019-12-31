#include "mem.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

// constante définie dans gcc seulement
#ifdef __BIGGEST_ALIGNMENT__
#define ALIGNMENT __BIGGEST_ALIGNMENT__
#else
#define ALIGNMENT 16
#endif
/* Code réalisé par GRANGER Oscar et COSOTTI Kévin
 * avec l'aide de ANDRIEUX Liam et REGOUIN Roman
 * l'alignement n'est pas pris en compte et
 * seul mem_fit_first a été réalisé
 */

/* La seule variable globale autorisée
 * On trouve à cette adresse la taille de la zone mémoire
 */
static void* memory_addr;

static inline void *get_system_memory_adr() {
	return memory_addr;
}

static inline size_t get_system_memory_size() {
	return *(size_t*)memory_addr;
}

struct cons{
	size_t size;
	mem_fit_function_t *mem_fit_fn;
};

struct fb {
	size_t size;
	struct fb* next;
	/* ... */
};

void mem_init(void* mem, size_t taille){
    memory_addr = mem;
    *(size_t*)memory_addr = taille;
	assert(mem == get_system_memory_adr());
	assert(taille == get_system_memory_size());
	struct fb *pfb=(struct fb*)(sizeof(struct cons)+(char*)mem);
	pfb->size=taille-sizeof(struct cons)-sizeof(struct fb);
	pfb->next=NULL;
	mem_fit(&mem_fit_first);
}

void mem_show_alloue(void (*print)(void*,size_t,int),char* debut, char* fin){
	while(debut<fin){
		print(debut+sizeof(size_t),*((size_t*)debut),0);
		debut+=*((size_t*)debut)+sizeof(size_t);
	}
}

void mem_show(void (*print)(void *, size_t, int)) {
	struct fb *start=memory_addr+sizeof(struct cons);
	char* cpt;
	if (start->size!=0) print(start,start->size,1);
	cpt=(char*)start+start->size+sizeof(struct fb);
	if (start->next!=NULL) mem_show_alloue(print,cpt,(char*)start->next);
	else mem_show_alloue(print,cpt,(char*)memory_addr+get_system_memory_size()-sizeof(struct cons));
	start=start->next;
	while (start!=NULL){
		print(start,start->size,1);
		cpt=(char*)start+start->size;
		if (start->next!=NULL) mem_show_alloue(print,cpt,(char*)start->next);
		else mem_show_alloue(print,cpt,(char*)memory_addr+get_system_memory_size()-sizeof(struct cons));
		start=start->next;
	}
}

mem_fit_function_t *mem_fit_fn;
void mem_fit(mem_fit_function_t *f) {
	mem_fit_fn=f;
}

void *mem_alloc(size_t taille) {
	int taille_max=get_system_memory_size()-sizeof(struct cons)-sizeof(struct fb);
	if (taille>=taille_max) return NULL;
	size_t taille_tot=taille+sizeof(size_t);
	struct fb *debut=(struct fb*)((char*)memory_addr+sizeof(struct cons));
	struct fb *fb=mem_fit_fn(memory_addr+sizeof(struct cons),taille_tot);
	if (fb==NULL) return NULL;
	if (fb==debut && fb->size>=taille_tot){
		if (taille+sizeof(struct fb)>fb->size){
			fb->size=0;
			*((size_t*)((char*)fb+sizeof(struct fb)))=taille;
			return (char*)fb+sizeof(struct fb)+sizeof(size_t);
		}
		struct fb *nouv=(struct fb*)((char*)fb+taille_tot+sizeof(struct fb));
		nouv->size=fb->size-taille-sizeof(size_t);
		nouv->next=fb->next;
		fb->next=nouv;
		fb->size=0;
		*((size_t*)((char*)fb+sizeof(struct fb)))=taille;
		return (char*)fb+sizeof(struct fb)+sizeof(size_t);
	}else if (fb->next!=NULL){
		if (taille+sizeof(struct fb)>fb->next->size){
			fb->size=0;
			*((size_t*)(char*)fb->next)=taille;
			struct fb* tmp=fb->next;
			fb->next=fb->next->next;
			return (char*)tmp+sizeof(size_t);
		}
		struct fb* nouv=(struct fb*)((char*)fb->next+taille+sizeof(size_t));
		nouv->size=fb->next->size-taille-sizeof(size_t);
		nouv->next=fb->next->next;
		*((size_t*)((char*)fb->next))=taille;
		struct fb *tmp=fb->next;
		fb->next=nouv;
		return (char*)tmp+sizeof(size_t);
	}else return NULL;
}

/* Fonction à faire dans un second temps
 * - utilisée par realloc() dans malloc_stub.c
 * - nécessaire pour remplacer l'allocateur de la libc
 * - donc nécessaire pour 'make test_ls'
 * Lire malloc_stub.c pour comprendre son utilisation
 * (ou en discuter avec l'enseignant)
 */
size_t mem_get_size(void *zone) {
	/* zone est une adresse qui a été retournée par mem_alloc() */

	/* la valeur retournée doit être la taille maximale que
	 * l'utilisateur peut utiliser dans cette zone */
	return *((size_t*)((char*)zone-sizeof(size_t)));
}


void mem_free(void* mem) {
	if (mem==NULL)return;
	struct fb* debut=(struct fb*)((char*)memory_addr+sizeof(struct cons));
	struct fb* list=debut;
	struct fb* prev=list;
	size_t taille=mem_get_size(mem)+sizeof(size_t);
	while(list!=NULL&&(char*)list<(char*)mem){
		prev=list;
		list=list->next;
	}
	struct fb* nouv=(struct fb*)((char*)mem-sizeof(size_t));
	if (list!=NULL&&(char*)mem+taille>=(char*)list){
		nouv->size=taille+list->size;
		nouv->next=list->next;
	}else{
		nouv->size=taille;
		nouv->next=list;
	}
	if (nouv!=NULL&&((char*)prev+prev->size==(char*)nouv||(prev==debut&&(char*)prev+prev->size==(char*)nouv-sizeof(struct fb)))){
		prev->size+=nouv->size;
		prev->next=nouv->next;
	}else prev->next=nouv;
}

struct fb* mem_fit_first(struct fb *list, size_t size) {
	struct fb *tmp=list;
	if (list->size>=size)return list;
	while ((list->next!=NULL)&&(list->next->size<size)){
		list=list->next;
	}
	if (tmp==list && list->next==NULL) return NULL;
	return list;
}

/* Fonctions facultatives
 * autres stratégies d'allocation
 */
struct fb* mem_fit_best(struct fb *list, size_t size) {
	return NULL;
}

struct fb* mem_fit_worst(struct fb *list, size_t size) {
	return NULL;
}
