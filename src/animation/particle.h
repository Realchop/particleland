#ifndef PARTICLE_H
#define PARTICLE_H

#include <stdbool.h>

typedef struct {
    double x;
    double y;
    double r;
    double tx;
    double ty;
} Particle;


void particle_rule_geometric_approach(double q, Particle *p);

bool particle_rule_circle_cutoff(double r, Particle *p);

void partcile_init(int width, int height, Particle *p);

void particle_multi_init(int width, int height, int len, Particle ps[]);


#endif
