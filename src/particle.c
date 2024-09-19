#include <stdlib.h>

#include "./particle.h"


void particle_rule_geometric_approach(double q, Particle *p) {
   p->x += (p->tx - p->x) / q; 
   p->y += (p->ty - p->y) / q; 
}

bool particle_rule_circle_cutoff(double r, Particle *p) {
    return (p->tx - p->x) * (p->tx - p->x) + (p->ty - p->y) * (p->ty - p->y) <= r*r;
}

void partcile_init(int width, int height, Particle *p) {
    p->x = rand()%width;    
    p->y = rand()%height;    
    p->r = 3;
    p->tx = rand()%width;
    p->ty = rand()%height;
}

void particle_multi_init(int width, int height, int len, Particle ps[]) {
    for(int i=0; i<len; ++i)
    {
        partcile_init(width, height, ps+i);
    }
}

