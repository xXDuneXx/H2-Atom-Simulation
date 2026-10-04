#pragma once
#include "Types.hpp"
#include <vector>
#include <random>
#include <SFML/Graphics.hpp>

// Утилиты (нужны в main.cpp для спавна нейтронов)
float uniform01(std::mt19937& rng);
sf::Vector2f randomUnitVector2D(std::mt19937& rng);

// Транспорт нейтронов
void stepNeutrons(
    std::vector<Neutron>& neutrons,
    std::vector<Gamma>& gammas,
    std::vector<Atom>& atoms,
    std::vector<Neutron>& delayedPool,
    const SpatialGrid& grid,
    sf::Vector2f boxSize,
    float dt,
    std::mt19937& rng);

void updateDelayedNeutrons(
    std::vector<Neutron>& activeNeutrons,
    std::vector<Neutron>& delayedPool,
    float dt,
    std::mt19937& rng);

void stepGammas(
    std::vector<Gamma>& gammas,
    sf::Vector2f boxSize,
    float dt);