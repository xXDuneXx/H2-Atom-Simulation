#pragma once
#include "Types.hpp"
#include <vector>
#include <random>
#include <SFML/Graphics.hpp>

// Утилиты (нужны в main.cpp для спавна нейтронов)
float uniform01(std::mt19937& rng);
sf::Vector2f randomUnitVector2D(std::mt19937& rng);

// ============================================================
// Параметры шага нейтронов, задаваемые уровнем.
// По умолчанию — «нейтральные» значения (L1–L5).
// ============================================================
struct NeutronStepConfig {
    // Гравитация (только L6)
    bool  neutronGravity = false;
    float gravityDirDeg = 90.0f;
    float gravityMag = 0.0f;

    // Трение (множитель к обычному демпфированию). 1.0 = как раньше,
    // 0.05 = почти нет трения (L6).
    float dragScale = 1.0f;

    // Отскок от колонн (только L6)
    bool  bouncePillars = false;
    float pillarRestitution = 0.4f;
    const std::vector<Pillar>* pillars = nullptr;

    // Не спавнить осколки деления (L6)
    bool skipFissionFragments = false;

    // Записывать визуальный след (L6)
    bool recordTrail = false;

    // Минимальная скорость: ниже — нейтрон «умер» (L6)
    float minSpeed = 0.0f;

    // Борта-неразрушимые стенки (L6)
    const std::vector<Barrier>* barriers = nullptr;
};

// Транспорт нейтронов
void stepNeutrons(
    std::vector<Neutron>& neutrons,
    std::vector<Gamma>& gammas,
    std::vector<Atom>& atoms,
    std::vector<Neutron>& delayedPool,
    const SpatialGrid& grid,
    sf::Vector2f boxSize,
    float dt,
    std::mt19937& rng,
    const NeutronStepConfig& cfg = NeutronStepConfig{});

void updateDelayedNeutrons(
    std::vector<Neutron>& activeNeutrons,
    std::vector<Neutron>& delayedPool,
    float dt,
    std::mt19937& rng);

void stepGammas(
    std::vector<Gamma>& gammas,
    sf::Vector2f boxSize,
    float dt);