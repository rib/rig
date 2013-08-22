# Particle engines

This repository contains a few examples of particle systems, with demonstration applications of each. Each particle system consists of a two part frontend + backend relationship:

* **The backend**: this is a common backend which supplies a means for constructing and drawing a particle system using Cogl. This is reponsible for assembling a cogl primitive from the particle vertices and managing the memory associated with them. The interface is defined in `pe/particle-engine.h`, and its implementation can be found in `pe/particle-engine.c`.

* **The frontend**: this is responsible for modelling the behaviour, movement and lifespan of the particles. It is here that the particles are assigned characterstics which defines how they behave, such as swarming patterns, flocking or deterministic movement. There are several frontend implementations for different purposes:

  * **Particle Swarm** - `pe/particle-swarm.h`
  * **Particle Emitter** - `pe/particle-emitter.h`
  * **Particle System** - `pe/particle-system.h`

## 1. Particle Swarm

This is a fairly standard emulation of the 1986 [Boids](http://en.wikipedia.org/wiki/Boids) program. It models each particle (or boid) as a simple entity whose behaviour is determiend by three rules:

1. **Cohesion** - a particle steers towards the center of mass of its surrounding particles.
2. **Alignment** - a particle attempts to mimic the movement of its surrounding particles.
3. **Separation** - a particle avoids other particles to prevent bumping into each other and to maintain a maximum swarm density.

Additionally, there are extra influences affecting a particle's movement:

4. **Boundaries** - particles are contained within a bounding area, and will steer to avoid going out of these bounds.
5. **Global forces** - a global force can be applied uniformly to each of the particles, for example to model the effects of strong wind or a current in water.
6. **Speed limits** - the speed of a particle is determined by it's size, and has minimum and maximum speeds enforced.

The implementation of these rules is contained within the `particle_apply_swarming_behaviour()` function in `pe/particle-swarm.c`. Additionally, there is a JavaScript+HTML5 implementation of this which models the flocking behaviour of birds, and can be found in the web directory.

### Examples
* `./examples/ants`
* `./examples/fish`
* `web/index.html`

## 2. Particle Emitter

### Examples
* `./examples/catherine_wheel`
* `./examples/fireworks`
* `./examples/fountains`
* `./examples/snow`

## 3. Particle System

A rough and ready model for simulating circular Kepler orbits of particles about a fixed center of mass.

### Examples
* `./examples/galaxy`
