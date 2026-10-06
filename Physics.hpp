#pragma once
#include "Types.hpp"
#include <random>
#include <string>

// Вспомогательные
sf::Vector2f rotateVec(const sf::Vector2f& v, float angle);
float valueToPos(float value);
float posToValue(float pos);
std::string formatSpeed(float v);
float tempCelsiusToPhysics(float c);
float tempCelsiusToSliderPos(float c);
float tempSliderPosToCelsius(float p);
std::string formatTempCelsius(float c);

// Физика
void computeInteraction(const std::vector<Atom>& atoms, int i, int j,
    float& fx, float& fy);
void updateBonds(std::vector<Atom>& atoms, const SpatialGrid& grid);
void updateHBonds(std::vector<Atom>& atoms, const SpatialGrid& grid,
    float physTemp, std::mt19937& rng);

// (вызывать после накопления парных сил, до интегрирования)
void applyHBondAngularForces(const std::vector<Atom>& atoms,
    std::vector<sf::Vector2f>& forces);

// НОВОЕ: жёсткий угол H-O-H в молекуле воды
void applyWaterAngleForces(const std::vector<Atom>& atoms,
    std::vector<sf::Vector2f>& forces);

// НОВОЕ: жёсткий угол H-O-O в пероксиде H2O2
void applyPeroxideAngleForces(const std::vector<Atom>& atoms,
    std::vector<sf::Vector2f>& forces);

// Фабрики
Atom makeHydrogen(sf::Vector2f pos, sf::Vector2f vel);
Atom makeOxygen(sf::Vector2f pos, sf::Vector2f vel);

// Коробка
float boxSizeToPos(float size);       // [BOX_MIN..BOX_MAX] → [0..1]
float boxSizePosToSize(float p);      // [0..1] → [BOX_MIN..BOX_MAX]

// Стены: столкновения со всеми атомами (вызывать после интегрирования)
void applyWallsToAtoms(std::vector<Atom>& atoms,
    const std::vector<Wall>& walls);

// НОВОЕ: реакция 2Na + 2H2O → 2NaOH + H2 + «взрыв»
void applySodiumWaterReaction(std::vector<Atom>& atoms);

// Столкновения атомов с неподвижными колоннами (уровень 4)
void applyPillarsToAtoms(std::vector<Atom>& atoms,
    const std::vector<Pillar>& pillars);

// НОВОЕ: столкновения атомов с неразрушимыми бортами (уровень 6).
// Без этого уран выпадает из «корзины» — барьеры действовали
// только на нейтроны.
void applyBarriersToAtoms(std::vector<Atom>& atoms,
    const std::vector<Barrier>& barriers);

// Готовая молекула воды: O + 2H, угол 104.5°, готовые ковалентные связи,
// общая скорость. baseIndex — индекс, с которого эти 3 атома будут
// добавлены в вектор atoms (используется для корректных bond-ссылок).
// Возвращает [0] = O, [1] = H1, [2] = H2.
std::array<Atom, 3> makeWaterMolecule(int baseIndex,
    sf::Vector2f center,
    float rotationRad,
    sf::Vector2f vel);