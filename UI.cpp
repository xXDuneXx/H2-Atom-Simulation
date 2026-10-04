#include "UI.hpp"
#include "Config.hpp"
#include "Physics.hpp"
#include "Localization.hpp"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <algorithm>

namespace {

    void drawGridLayer(sf::RenderTarget& target, const sf::View& camera,
        float spacing, sf::Color baseColor, float alpha) {
        if (alpha <= 0.005f || spacing <= 0.0f) return;
        sf::Color color = baseColor;
        color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);

        sf::VertexArray lines(sf::PrimitiveType::Lines);
        sf::Vector2f center = camera.getCenter();
        sf::Vector2f size = camera.getSize();
        float left = center.x - size.x / 2.0f;
        float right = center.x + size.x / 2.0f;
        float top = center.y - size.y / 2.0f;
        float bottom = center.y + size.y / 2.0f;
        float startX = std::floor(left / spacing) * spacing;
        float startY = std::floor(top / spacing) * spacing;

        for (float x = startX; x <= right; x += spacing) {
            lines.append(sf::Vertex({ x, top }, color));
            lines.append(sf::Vertex({ x, bottom }, color));
        }
        for (float y = startY; y <= bottom; y += spacing) {
            lines.append(sf::Vertex({ left, y }, color));
            lines.append(sf::Vertex({ right, y }, color));
        }
        target.draw(lines);
    }

    // Проверка: не под курсором ли верхняя панель / открытое меню спавна.
    bool mouseOverUI(sf::Vector2i mp, sf::Vector2u winSize,
        const UIState& ui, float S) {
        if (mp.y < (int)PANEL_HEIGHT) return true;
        if (ui.spawnMenuOpen) {
            float mLeft = MENU_LEFT * S;
            float mTop = PANEL_HEIGHT + 10.0f * S;
            float mWidth = MENU_WIDTH * S;
            float mHeaderH = MENU_HEADER_H * S;
            float mRowH = MENU_ROW_H * S;
            float mFooterH = MENU_FOOTER_H * S;
            float mTotalH = mHeaderH + (float)ATOM_TYPES.size() * mRowH + mFooterH;
            if (sf::FloatRect({ mLeft, mTop }, { mWidth, mTotalH }).contains(
                { (float)mp.x, (float)mp.y })) {
                return true;
            }
        }
        return false;
    }

    // ============================================================
    // Температурная виньетка
    // ============================================================
    void drawVignette(sf::RenderTarget& target, sf::Vector2u winSize,
        sf::Color color, float intensity) {
        if (intensity <= 0.001f) return;

        sf::Vector2f center(winSize.x * 0.5f, winSize.y * 0.5f);
        float maxR = std::sqrt(center.x * center.x + center.y * center.y);

        const int SEGMENTS = 64;
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan, SEGMENTS + 2);

        sf::Color centerColor = color;
        centerColor.a = 0;
        fan[0] = sf::Vertex(center, centerColor);

        sf::Color edgeColor = color;
        edgeColor.a = static_cast<std::uint8_t>(
            std::clamp(intensity * 255.0f, 0.0f, 255.0f));

        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (float)i / SEGMENTS * 2.0f * 3.14159265f;
            float x = center.x + std::cos(angle) * maxR;
            float y = center.y + std::sin(angle) * maxR;
            fan[i + 1] = sf::Vertex({ x, y }, edgeColor);
        }
        target.draw(fan);
    }

    // Линейная интерполяция цветов
    sf::Color lerpColor(const sf::Color& a, const sf::Color& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return sf::Color(
            static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<std::uint8_t>(a.b + (b.b - a.b) * t));
    }

    // ============================================================
    // Кнопка: принимает sf::String
    // ============================================================
    void drawButton(sf::RenderTarget& target, const sf::Font& font, bool fontLoaded,
        const sf::FloatRect& rect, const sf::String& label,
        bool hovered, float S)
    {
        sf::RectangleShape bg({ rect.size.x, rect.size.y });
        bg.setPosition(rect.position);
        if (hovered) {
            bg.setFillColor(sf::Color(70, 120, 200, 235));
            bg.setOutlineColor(sf::Color(150, 200, 255, 255));
        }
        else {
            bg.setFillColor(sf::Color(35, 40, 50, 220));
            bg.setOutlineColor(sf::Color(80, 90, 110, 255));
        }
        bg.setOutlineThickness(2.0f * S);
        target.draw(bg);

        if (fontLoaded) {
            int fontSize = (int)std::round(24.0f * S);
            sf::Text text(font, label, fontSize);
            text.setFillColor(sf::Color::White);
            text.setStyle(sf::Text::Bold);
            sf::FloatRect tb = text.getLocalBounds();
            text.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                            tb.position.y + tb.size.y * 0.5f });
            text.setPosition({ rect.position.x + rect.size.x * 0.5f,
                              rect.position.y + rect.size.y * 0.5f });
            target.draw(text);
        }
    }

    // ============================================================
    // Локализованное имя элемента — теперь возвращает sf::String.
    // ============================================================
    inline sf::String locAtomName(int id, Language lang) {
        switch (id) {
        case 0: return tr(Loc::AtomHydrogen, lang);
        case 1: return tr(Loc::AtomOxygen, lang);
        case 2: return tr(Loc::AtomChlorine, lang);
        case 3: return tr(Loc::AtomUranium, lang);
        case 4: return tr(Loc::AtomSodium, lang);
        case 5: return tr(Loc::AtomBarium, lang);
        case 6: return tr(Loc::AtomKrypton, lang);
        case 7: return tr(Loc::AtomNeutron, lang);
        }
        return sf::String(ATOM_TYPES[id].name);
    }
} // namespace

sf::FloatRect pauseMenuButtonRect(sf::Vector2u winSize, int idx) {
    float S = std::max(1.0f, (float)winSize.y / 1080.0f);
    float btnW = PAUSE_MENU_BTN_W * S;
    float btnH = PAUSE_MENU_BTN_H * S;
    float gap = PAUSE_MENU_BTN_GAP * S;
    float cx = (float)winSize.x * PAUSE_MENU_CX_FRAC;
    float startY = (float)winSize.y * PAUSE_MENU_START_Y_FRAC;
    return sf::FloatRect({ cx - btnW * 0.5f, startY + idx * (btnH + gap) },
        { btnW, btnH });
}

sf::FloatRect nextLevelButtonRect(sf::Vector2u winSize, float panelH) {
    // Кнопка "Next Level" живёт ВНУТРИ панели задач —
    // в правом верхнем углу экрана, снизу от панели.
    // Высота панели зависит от уровня: L1=400, L2=480, L3=460.
    float S = std::max(1.0f, (float)winSize.y / 1080.0f);
    const float tW = 360.0f * S;
    const float tMargin = STATS_MARGIN * S;
    const float tX = (float)winSize.x - tW - tMargin;
    const float tY = PANEL_HEIGHT + 10.0f * S;
    const float tH = panelH * S;

    const float bw = tW - 24.0f * S;
    const float bh = 30.0f * S;
    const float bx = tX + 12.0f * S;
    const float by = tY + tH - 12.0f * S - bh;
    return sf::FloatRect({ bx, by }, { bw, bh });
}

void drawEverything(sf::RenderTarget& target,
    sf::Vector2i mousePixel,
    const sf::Font& font, bool fontLoaded,
    const std::vector<Atom>& atoms,
    const std::vector<Wall>& walls,
    const std::vector<Neutron>& neutrons,
    const std::vector<Gamma>& gammas,
    const sf::View& camera,
    UIState& ui,
    sf::Vector2f boxSize,
    DrawPass pass)
{
    sf::Vector2u winSize = target.getSize();
    float S = std::max(1.0f, (float)winSize.y / 1080.0f);
    float worldPerPixel = camera.getSize().x / (float)winSize.x;
    float halfX = boxSize.x * 0.5f;
    float halfY = boxSize.y * 0.5f;

    // ============================================================
    // Видимые границы мира — для отсечения всего, что за камерой
    // ============================================================
    sf::Vector2f camCenter = camera.getCenter();
    sf::Vector2f camSize = camera.getSize();
    const float visLeft = camCenter.x - camSize.x * 0.5f;
    const float visRight = camCenter.x + camSize.x * 0.5f;
    const float visTop = camCenter.y - camSize.y * 0.5f;
    const float visBottom = camCenter.y + camSize.y * 0.5f;

    // ============================================================
    // Температурный градиент: виньетка + оттенок сетки
    // ============================================================
    const float TINT_NEUTRAL_C = 15.0f;
    const float TINT_SATURATE_DELTA_C = 42.0f;
    const float TINT_MAX_ALPHA = 0.13f;

    const float tempC = ui.targetTempCelsius;
    const float delta = tempC - TINT_NEUTRAL_C;
    float coldS = (delta < 0.0f)
        ? std::clamp(-delta / TINT_SATURATE_DELTA_C, 0.0f, 1.0f)
        : 0.0f;
    float hotS = (delta > 0.0f)
        ? std::clamp(delta / TINT_SATURATE_DELTA_C, 0.0f, 1.0f)
        : 0.0f;

    sf::Color vignetteColor = (coldS > 0.0f)
        ? sf::Color(70, 130, 220)
        : sf::Color(240, 120, 40);
    float vignetteIntensity = std::max(coldS, hotS) * TINT_MAX_ALPHA;

    sf::Color gridBaseColor = sf::Color(18, 18, 18);
    if (coldS > 0.0f) {
        gridBaseColor = lerpColor(sf::Color(18, 18, 18), sf::Color(22, 26, 37), coldS);
    }
    else if (hotS > 0.0f) {
        gridBaseColor = lerpColor(sf::Color(18, 18, 18), sf::Color(35, 24, 20), hotS);
    }

    // ============================================================
    // Мир (view = camera) — рисуется для DrawPass::World / All
    // ============================================================
    if (pass != DrawPass::UI) {
        target.setView(camera);
        target.clear(sf::Color(10, 10, 10));

        {
            sf::RectangleShape boxShape(boxSize);
            boxShape.setOrigin({ boxSize.x / 2.0f, boxSize.y / 2.0f });
            boxShape.setPosition({ 0.0f, 0.0f });
            boxShape.setFillColor(sf::Color::Transparent);
            boxShape.setOutlineColor(sf::Color(140, 140, 140));
            boxShape.setOutlineThickness(2.0f * worldPerPixel);

            const float gridSpacing = 1.0f;
            const float pixelsPerCell = gridSpacing / worldPerPixel;
            float gridAlpha = std::clamp((pixelsPerCell - 2.0f) / 6.0f, 0.0f, 1.0f);
            drawGridLayer(target, camera, gridSpacing, gridBaseColor, gridAlpha);

            target.draw(boxShape);
        }

        // Связи
        {
            sf::VertexArray bonds(sf::PrimitiveType::Lines);

            const sf::Color colHH(204, 204, 255, 220);
            const sf::Color colOO(196, 68, 68, 230);
            const sf::Color colHO(220, 180, 180, 220);
            const sf::Color colClCl(94, 170, 77, 230);
            const sf::Color colHCl(180, 220, 160, 220);
            const sf::Color colNaCl(190, 170, 240, 230);

            for (size_t i = 0; i < atoms.size(); ++i) {
                std::vector<int> partners;
                for (int j : atoms[i].bonds) {
                    if (j <= (int)i) continue;
                    bool already = false;
                    for (int p : partners) if (p == j) { already = true; break; }
                    if (!already) partners.push_back(j);
                }

                for (int j : partners) {
                    int order = 0;
                    for (int k : atoms[i].bonds) if (k == j) order++;

                    sf::Color col;
                    const int e1 = atoms[i].elementId;
                    const int e2 = atoms[j].elementId;
                    if (e1 == 1 && e2 == 1)      col = colOO;
                    else if (e1 == 0 && e2 == 0) col = colHH;
                    else if (e1 == 2 && e2 == 2) col = colClCl;
                    else if ((e1 == 0 && e2 == 2) || (e1 == 2 && e2 == 0)) col = colHCl;
                    else if ((e1 == 4 && e2 == 2) || (e1 == 2 && e2 == 4)) col = colNaCl;
                    else                         col = colHO;

                    sf::Vector2f pi = atoms[i].pos;
                    sf::Vector2f pj = atoms[j].pos;

                    if (order >= 2) {
                        sf::Vector2f d = pj - pi;
                        float len = std::sqrt(d.x * d.x + d.y * d.y);
                        if (len > 1e-6f) {
                            sf::Vector2f n(-d.y / len, d.x / len);
                            float offset = atoms[i].radius * 0.45f;

                            sf::Vector2f a1 = pi + n * offset;
                            sf::Vector2f a2 = pj + n * offset;
                            sf::Vector2f b1 = pi - n * offset;
                            sf::Vector2f b2 = pj - n * offset;

                            bonds.append(sf::Vertex(a1, col));
                            bonds.append(sf::Vertex(a2, col));
                            bonds.append(sf::Vertex(b1, col));
                            bonds.append(sf::Vertex(b2, col));
                        }
                    }
                    else {
                        bonds.append(sf::Vertex(pi, col));
                        bonds.append(sf::Vertex(pj, col));
                    }
                }
            }
            target.draw(bonds);
        }

        // ============================================================
        // Водородные связи — пунктиром
        // ============================================================
        {
            sf::VertexArray hbVA(sf::PrimitiveType::Lines);
            const sf::Color hbWater(140, 200, 240, 180);
            const sf::Color hbHCl(150, 220, 150, 180);

            const int DASHES = 6;
            for (size_t i = 0; i < atoms.size(); ++i) {
                for (int j : atoms[i].hbonds) {
                    if (j < 0 || j <= (int)i) continue;
                    if (j >= (int)atoms.size()) continue;
                    if (!isHBondedTo(atoms[j], (int)i)) continue;

                    sf::Color col = hbWater;
                    int e1 = atoms[i].elementId;
                    int e2 = atoms[j].elementId;
                    if ((e1 == 0 && e2 == 2) || (e1 == 2 && e2 == 0))
                        col = hbHCl;

                    sf::Vector2f pi = atoms[i].pos;
                    sf::Vector2f pj = atoms[j].pos;
                    for (int s = 0; s < DASHES; s += 2) {
                        float t0 = (float)s / DASHES;
                        float t1 = (float)(s + 1) / DASHES;
                        sf::Vector2f a = pi + (pj - pi) * t0;
                        sf::Vector2f b = pi + (pj - pi) * t1;
                        hbVA.append(sf::Vertex(a, col));
                        hbVA.append(sf::Vertex(b, col));
                    }
                }
            }
            target.draw(hbVA);
        }

        // Аура выделения
        for (const auto& a : atoms) {
            if (!a.selected) continue;
            float auraR = a.radius * 1.7f;
            sf::CircleShape aura(auraR);
            aura.setOrigin({ auraR, auraR });
            aura.setPosition(a.pos);
            aura.setFillColor(sf::Color(255, 140, 0, 60));
            aura.setOutlineColor(sf::Color(255, 140, 0, 200));
            aura.setOutlineThickness(1.0f * worldPerPixel);
            target.draw(aura);
        }

        // Зелёная аура на «правильных» одиночных Cl в L3 фазе 2.
        // Считается «правильным», если у Cl нет ни одной ковалентной
        // связи: значит, он не превратился в HCl или Cl2.
        if (ui.campaignMode && ui.campaignLevel == 3 && ui.taskPhase == 2) {
            for (const auto& a : atoms) {
                if (a.elementId != 2) continue;   // Cl
                if (countBonds(a) != 0) continue;
                if (a.selected) continue;         // не перекрываем ауру выделения

                float auraR = a.radius * 1.55f;
                sf::CircleShape aura(auraR);
                aura.setOrigin({ auraR, auraR });
                aura.setPosition(a.pos);
                aura.setFillColor(sf::Color(80, 220, 120, 55));
                aura.setOutlineColor(sf::Color(100, 240, 140, 190));
                aura.setOutlineThickness(1.5f * worldPerPixel);
                target.draw(aura);
            }
        }

        // Атомы
        const float INSIDE_START_PX = 250.0f;
        const float INSIDE_FULL_PX = 500.0f;

        for (const auto& a : atoms) {
            float cullR = a.radius * 2.0f;
            if (a.pos.x + cullR < visLeft || a.pos.x - cullR > visRight ||
                a.pos.y + cullR < visTop || a.pos.y - cullR > visBottom) {
                continue;
            }

            float screenRadius = a.radius / worldPerPixel;
            float t = ui.showDetailedAtoms
                ? std::clamp((screenRadius - INSIDE_START_PX) /
                    (INSIDE_FULL_PX - INSIDE_START_PX), 0.0f, 1.0f)
                : 0.0f;

            sf::Color fill = a.color;
            fill.r = static_cast<std::uint8_t>(fill.r * (1.0f - t));
            fill.g = static_cast<std::uint8_t>(fill.g * (1.0f - t));
            fill.b = static_cast<std::uint8_t>(fill.b * (1.0f - t));

            sf::CircleShape c(a.radius);
            c.setFillColor(fill);
            c.setOutlineColor(atomOutlineColor(a.elementId));
            float outlineW = std::min(3.5f * worldPerPixel, a.radius * 0.6f);
            c.setOutlineThickness(outlineW);
            c.setOrigin({ a.radius, a.radius });
            c.setPosition(a.pos);
            target.draw(c);

            // Детальное ядро
            if (ui.showDetailedAtoms && t > 0.005f && !a.nucleons.empty()) {
                std::uint8_t alpha = static_cast<std::uint8_t>(t * 255.0f);

                const float nucWorldR = NUCLEON_RADIUS * NUCLEUS_SIZE_SCALE;
                const float nucScreenR = nucWorldR / worldPerPixel;
                const float nucOutline = std::max(0.25f * worldPerPixel,
                    nucWorldR * 0.075f);

                for (const auto& nuc : a.nucleons) {
                    sf::Vector2f worldP = a.pos + nuc.relPos;
                    float nucCull = nucWorldR * 1.5f;
                    if (worldP.x + nucCull < visLeft || worldP.x - nucCull > visRight ||
                        worldP.y + nucCull < visTop || worldP.y - nucCull > visBottom) {
                        continue;
                    }

                    sf::CircleShape nc(nucWorldR);
                    nc.setOrigin({ nucWorldR, nucWorldR });
                    nc.setPosition(worldP);

                    sf::Color fill, outline;
                    if (nuc.type == 0) {
                        fill = sf::Color(220, 80, 80, alpha);
                        outline = sf::Color(176, 64, 64, alpha);
                    }
                    else {
                        fill = sf::Color(240, 240, 245, alpha);
                        outline = sf::Color(192, 192, 196, alpha);
                    }
                    nc.setFillColor(fill);
                    nc.setOutlineColor(outline);
                    nc.setOutlineThickness(nucOutline);
                    target.draw(nc);
                }
            }
        }

        // Нейтроны
        const float freeNeutronWorldR = NUCLEON_RADIUS * NUCLEUS_SIZE_SCALE;
        for (const auto& n : neutrons) {
            float screenR = freeNeutronWorldR / worldPerPixel;
            if (screenR < 2.0f) screenR = 2.0f;
            float worldR = screenR * worldPerPixel;

            sf::CircleShape c(worldR);
            c.setOrigin({ worldR, worldR });
            c.setPosition(n.pos);
            c.setFillColor(sf::Color(240, 240, 245));
            c.setOutlineColor(sf::Color(192, 192, 196));
            c.setOutlineThickness(0.25f * worldPerPixel);
            target.draw(c);

            sf::Vertex line[] = {
                sf::Vertex(n.pos, sf::Color(200, 220, 255, 200)),
                sf::Vertex(n.pos - n.dir * 0.8f, sf::Color(200, 220, 255, 0))
            };
            target.draw(line, 2, sf::PrimitiveType::Lines);
        }

        // Гамма — жёлтые вспышки
        for (const auto& g : gammas) {
            sf::CircleShape c(0.12f);
            c.setOrigin({ 0.12f, 0.12f });
            c.setPosition(g.pos);
            c.setFillColor(sf::Color(255, 240, 150));
            target.draw(c);
        }

        // ============================================================
        // Стены
        // ============================================================
        {
            for (const auto& w : walls) {
                sf::Vector2f d = w.b - w.a;
                float len = std::sqrt(d.x * d.x + d.y * d.y);
                if (len < 1e-6f) continue;
                sf::Vector2f n(-d.y / len * WALL_THICKNESS * 0.5f,
                    d.x / len * WALL_THICKNESS * 0.5f);

                sf::Color fill = w.selected
                    ? sf::Color(255, 200, 100, 255)
                    : sf::Color(160, 160, 170, 255);
                sf::Color edge = w.selected
                    ? sf::Color(255, 240, 200, 255)
                    : sf::Color(200, 200, 210, 255);

                sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
                quad[0] = sf::Vertex(w.a + n, fill);
                quad[1] = sf::Vertex(w.a - n, fill);
                quad[2] = sf::Vertex(w.b + n, fill);
                quad[3] = sf::Vertex(w.b - n, fill);
                target.draw(quad);

                sf::VertexArray outline(sf::PrimitiveType::Lines, 4);
                outline[0] = sf::Vertex(w.a + n, edge);
                outline[1] = sf::Vertex(w.b + n, edge);
                outline[2] = sf::Vertex(w.a - n, edge);
                outline[3] = sf::Vertex(w.b - n, edge);
                target.draw(outline);
            }
        }

        // Индикатор направления/скорости спавна
        if (ui.spawnDragActive) {
            sf::Vector2i mp = mousePixel;
            sf::Vector2f mouseWorld = target.mapPixelToCoords(mp, camera);
            sf::Vector2f d = mouseWorld - ui.spawnDragOrigin;
            float len = std::sqrt(d.x * d.x + d.y * d.y);

            if (len > 1e-6f) {
                sf::Color arrowCol(120, 220, 255, 220);

                sf::Vertex line[] = {
                    sf::Vertex(ui.spawnDragOrigin, arrowCol),
                    sf::Vertex(mouseWorld, arrowCol)
                };
                target.draw(line, 2, sf::PrimitiveType::Lines);

                float angle = std::atan2(d.y, d.x);
                float headLen = 0.35f;
                float headAngle = 0.5f;
                sf::Vector2f p1 = mouseWorld - sf::Vector2f(
                    std::cos(angle - headAngle), std::sin(angle - headAngle)) * headLen;
                sf::Vector2f p2 = mouseWorld - sf::Vector2f(
                    std::cos(angle + headAngle), std::sin(angle + headAngle)) * headLen;
                sf::Vertex head[] = {
                    sf::Vertex(mouseWorld, arrowCol),
                    sf::Vertex(p1, arrowCol),
                    sf::Vertex(mouseWorld, arrowCol),
                    sf::Vertex(p2, arrowCol)
                };
                target.draw(head, 4, sf::PrimitiveType::Lines);
            }
        }

        // Кольцо фокуса
        if (ui.focusedAtomIndex >= 0 && ui.focusedAtomIndex < (int)atoms.size()) {
            const auto& fa = atoms[ui.focusedAtomIndex];
            float ringR = fa.radius * 2.2f;
            sf::CircleShape ring(ringR);
            ring.setOrigin({ ringR, ringR });
            ring.setPosition(fa.pos);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(sf::Color(120, 200, 255, 210));
            ring.setOutlineThickness(1.2f * worldPerPixel);
            target.draw(ring);
        }

        // Hover-preview спавна
        if (ui.selectedSpawnType >= 0) {
            sf::Vector2i mp = mousePixel;
            sf::Vector2f worldPos;
            bool show = false;

            bool pendingSpawn = ui.spawnDragActive
                && ui.spawnDragAtomIndices.empty()
                && ui.spawnDragNeutronIndices.empty();

            if (pendingSpawn) {
                worldPos = ui.spawnDragOrigin;
                show = true;
            }
            else if (!ui.spawnDragActive && !mouseOverUI(mp, winSize, ui, S)) {
                worldPos = target.mapPixelToCoords(mp, camera);
                show = true;
            }

            if (show) {
                const AtomType& t = ATOM_TYPES[ui.selectedSpawnType];
                float halfXb = ui.boxSizeX / 2.0f;
                float halfYb = ui.boxSizeY / 2.0f;
                worldPos.x = std::clamp(worldPos.x, -halfXb + t.radius, halfXb - t.radius);
                worldPos.y = std::clamp(worldPos.y, -halfYb + t.radius, halfYb - t.radius);

                sf::Color previewFill = t.color;
                previewFill.a = 110;
                sf::Color previewOutline(200, 200, 200, 180);

                sf::CircleShape preview(t.radius);
                preview.setOrigin({ t.radius, t.radius });
                preview.setPosition(worldPos);
                preview.setFillColor(previewFill);
                preview.setOutlineColor(previewOutline);
                preview.setOutlineThickness(2.0f * worldPerPixel);
                target.draw(preview);
            }
        }

        // Copy-preview
        if (ui.copyMode && !ui.copyTemplates.empty()) {
            sf::Vector2f worldPos;
            if (ui.copyPendingSpawn && ui.spawnDragActive) {
                worldPos = ui.spawnDragOrigin;
            }
            else {
                sf::Vector2i mp = mousePixel;
                worldPos = target.mapPixelToCoords(mp, camera);
            }
            float halfXb = ui.boxSizeX / 2.0f;
            float halfYb = ui.boxSizeY / 2.0f;
            for (size_t i = 0; i < ui.copyTemplates.size(); ++i) {
                sf::Vector2f rotated = rotateVec(ui.copyOffsets[i], ui.copyRotation);
                sf::Vector2f p = worldPos + rotated;
                p.x = std::clamp(p.x, -halfXb + ui.copyTemplates[i].radius, halfXb - ui.copyTemplates[i].radius);
                p.y = std::clamp(p.y, -halfYb + ui.copyTemplates[i].radius, halfYb - ui.copyTemplates[i].radius);

                sf::Color fill = ui.copyTemplates[i].color;
                fill.a = 110;

                sf::CircleShape c(ui.copyTemplates[i].radius);
                c.setOrigin({ ui.copyTemplates[i].radius, ui.copyTemplates[i].radius });
                c.setPosition(p);
                c.setFillColor(fill);
                c.setOutlineColor(sf::Color(120, 220, 255, 200));
                c.setOutlineThickness(2.0f * worldPerPixel);
                target.draw(c);
            }
        }

        // Превью стены в процессе рисования
        if (ui.isDrawingWall) {
            sf::Vector2f d = ui.wallDrawCurrent - ui.wallDrawStart;
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len > 1e-6f) {
                sf::Vector2f n(-d.y / len * WALL_THICKNESS * 0.5f,
                    d.x / len * WALL_THICKNESS * 0.5f);
                sf::Color fill(255, 230, 100, 140);
                sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
                quad[0] = sf::Vertex(ui.wallDrawStart + n, fill);
                quad[1] = sf::Vertex(ui.wallDrawStart - n, fill);
                quad[2] = sf::Vertex(ui.wallDrawCurrent + n, fill);
                quad[3] = sf::Vertex(ui.wallDrawCurrent - n, fill);
                target.draw(quad);
            }
        }

        // Температурная виньетка
        {
            sf::View screenView(sf::FloatRect({ 0.0f, 0.0f },
                { (float)winSize.x, (float)winSize.y }));
            target.setView(screenView);
            drawVignette(target, winSize, vignetteColor, vignetteIntensity);
        }

    } // конец блока DrawPass::World / All

    if (pass == DrawPass::World) return;

    // ============================================================
    // Экранный слой (UI) — рисуется для DrawPass::UI / All.
    // ============================================================
    {
        sf::View screenView(sf::FloatRect({ 0.0f, 0.0f },
            { (float)winSize.x, (float)winSize.y }));
        target.setView(screenView);

        // Маркер направления на коробку
        {
            sf::Vector2f viewCenter = camera.getCenter();
            sf::Vector2f viewSize = camera.getSize();
            float halfXb = boxSize.x / 2.0f;
            float halfYb = boxSize.y / 2.0f;
            bool boxVisible = !(halfXb < viewCenter.x - viewSize.x / 2.0f ||
                -halfXb > viewCenter.x + viewSize.x / 2.0f ||
                halfYb < viewCenter.y - viewSize.y / 2.0f ||
                -halfYb > viewCenter.y + viewSize.y / 2.0f);

            if (!boxVisible) {
                sf::Vector2f screenCenter(winSize.x / 2.0f, winSize.y / 2.0f);
                sf::Vector2f worldDir = -viewCenter;
                sf::Vector2f screenDir(
                    worldDir.x / viewSize.x * (float)winSize.x,
                    worldDir.y / viewSize.y * (float)winSize.y
                );
                float len = std::sqrt(screenDir.x * screenDir.x + screenDir.y * screenDir.y);
                if (len > 1e-6f) {
                    screenDir /= len;
                    float margin = 40.0f;
                    float halfW = winSize.x / 2.0f - margin;
                    float halfH = winSize.y / 2.0f - margin;
                    float tX = (std::abs(screenDir.x) > 1e-6f) ? halfW / std::abs(screenDir.x) : 1e9f;
                    float tY = (std::abs(screenDir.y) > 1e-6f) ? halfH / std::abs(screenDir.y) : 1e9f;
                    float tEdge = std::min(tX, tY);
                    sf::Vector2f markerPos = screenCenter + screenDir * tEdge;
                    float angleDeg = std::atan2(screenDir.y, screenDir.x) * 180.0f / 3.14159265f;

                    const float markerRadius = 14.0f;
                    sf::CircleShape directionMarker(markerRadius, 3);
                    directionMarker.setOrigin({ markerRadius, markerRadius });
                    directionMarker.setFillColor(sf::Color(180, 180, 180));
                    directionMarker.setPosition(markerPos);
                    directionMarker.setRotation(sf::degrees(angleDeg + 90.0f));
                    target.draw(directionMarker);
                }
            }
        }

        // Индикатор скорости при drag-спавне
        if (ui.spawnDragActive && fontLoaded) {
            sf::Vector2f camC = camera.getCenter();
            sf::Vector2f camS = camera.getSize();
            float W = (float)winSize.x;
            float H = (float)winSize.y;

            sf::Vector2f mouseWorld(
                camC.x + ((float)mousePixel.x - W * 0.5f) / W * camS.x,
                camC.y + ((float)mousePixel.y - H * 0.5f) / H * camS.y);

            sf::Vector2f d = mouseWorld - ui.spawnDragOrigin;
            float dragLen = std::sqrt(d.x * d.x + d.y * d.y);

            sf::Vector2i originPixel(
                (int)std::round((ui.spawnDragOrigin.x - camC.x) / camS.x * W + W * 0.5f),
                (int)std::round((ui.spawnDragOrigin.y - camC.y) / camS.y * H + H * 0.5f));
            float dpx = std::sqrt(
                (float)(mousePixel.x - originPixel.x) * (mousePixel.x - originPixel.x) +
                (float)(mousePixel.y - originPixel.y) * (mousePixel.y - originPixel.y));

            if (dpx > SPAWN_DRAG_MIN_PX && dragLen > 1e-6f) {
                float speed = std::min(dragLen * SPAWN_SPEED_K, SPAWN_SPEED_MAX);
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.1f u/s", speed);
                int fs = (int)std::round(14.0f * S);
                sf::Text spd(font, buf, fs);
                spd.setFillColor(sf::Color(150, 230, 255));
                spd.setStyle(sf::Text::Bold);
                sf::FloatRect sb = spd.getLocalBounds();
                spd.setOrigin({ sb.position.x + sb.size.x * 0.5f,
                                sb.position.y + sb.size.y * 0.5f });
                spd.setPosition({ (float)mousePixel.x + 44.0f * S,
                                  (float)mousePixel.y - 26.0f * S });
                target.draw(spd);
            }
        }

        // Рамка выделения
        if (ui.lmbIsDown && ui.lmbIsDragging) {
            float x1 = (float)std::min(ui.lmbDownPixel.x, ui.lmbCurrentPixel.x);
            float y1 = (float)std::min(ui.lmbDownPixel.y, ui.lmbCurrentPixel.y);
            float x2 = (float)std::max(ui.lmbDownPixel.x, ui.lmbCurrentPixel.x);
            float y2 = (float)std::max(ui.lmbDownPixel.y, ui.lmbCurrentPixel.y);
            sf::RectangleShape rect({ x2 - x1, y2 - y1 });
            rect.setPosition({ x1, y1 });
            rect.setFillColor(sf::Color(120, 180, 255, 40));
            rect.setOutlineColor(sf::Color(120, 180, 255, 220));
            rect.setOutlineThickness(1.0f);
            target.draw(rect);
        }

        // Метки символа элемента на атомах
        if (fontLoaded && !atoms.empty()) {
            sf::Text label(font, "H", 12);
            label.setFillColor(sf::Color(30, 30, 30));
            label.setStyle(sf::Text::Bold);

            sf::Vector2f camCenter2 = camera.getCenter();
            sf::Vector2f camSize2 = camera.getSize();
            float W = (float)winSize.x;
            float H = (float)winSize.y;

            for (const auto& a : atoms) {
                float sx = (a.pos.x - camCenter2.x) / camSize2.x * W + W / 2.0f;
                float sy = (a.pos.y - camCenter2.y) / camSize2.y * H + H / 2.0f;
                if (sx < -20.0f || sx > W + 20.0f || sy < -20.0f || sy > H + 20.0f) continue;

                float screenRadius = a.radius / camSize2.x * W;
                float t = std::clamp((screenRadius - 250.0f) / 250.0f, 0.0f, 1.0f);
                if (t > 0.95f) continue;

                int size = std::max(6, (int)std::round(screenRadius * 1.3f));
                label.setCharacterSize(size);
                label.setString(ATOM_TYPES[a.elementId].symbol);
                label.setFillColor(sf::Color(30, 30, 30,
                    static_cast<std::uint8_t>((1.0f - t) * 255.0f)));

                sf::FloatRect lb = label.getLocalBounds();
                label.setOrigin({ lb.position.x + lb.size.x / 2.0f,
                                  lb.position.y + lb.size.y / 2.0f });
                label.setPosition({ sx, sy });
                target.draw(label);
            }
        }

        // ============================================================
        // Метки P / n на нуклонах
        // ============================================================
        if (fontLoaded && ui.showDetailedAtoms) {
            const float NUCLEON_WORLD_R = NUCLEON_RADIUS * NUCLEUS_SIZE_SCALE;
            const float NUCLEON_SCREEN_R = NUCLEON_WORLD_R / worldPerPixel;

            if (NUCLEON_SCREEN_R >= 5.0f) {
                const float LABEL_SAFETY_MAX_PX = 500.0f;
                const float LABEL_MIN_PX = 6.0f;

                float labelPx = NUCLEON_SCREEN_R * 1.3f;
                if (labelPx > LABEL_SAFETY_MAX_PX) labelPx = LABEL_SAFETY_MAX_PX;
                if (labelPx < LABEL_MIN_PX) labelPx = LABEL_MIN_PX;
                int charSize = (int)std::round(labelPx);

                sf::Text nucLabel(font, "P", charSize);
                nucLabel.setStyle(sf::Text::Bold);

                sf::Vector2f camCenter2 = camera.getCenter();
                sf::Vector2f camSize2 = camera.getSize();
                float W = (float)winSize.x;
                float H = (float)winSize.y;

                for (const auto& a : atoms) {
                    float cullR = a.radius * 2.0f;
                    if (a.pos.x + cullR < visLeft || a.pos.x - cullR > visRight ||
                        a.pos.y + cullR < visTop || a.pos.y - cullR > visBottom) {
                        continue;
                    }

                    float screenRadius = a.radius / worldPerPixel;
                    float t = std::clamp((screenRadius - 250.0f) / 250.0f, 0.0f, 1.0f);
                    if (t < 0.03f) continue;
                    std::uint8_t alpha = static_cast<std::uint8_t>(t * 255.0f);

                    for (const auto& nuc : a.nucleons) {
                        sf::Vector2f worldP = a.pos + nuc.relPos;
                        float sx = (worldP.x - camCenter2.x) / camSize2.x * W + W / 2.0f;
                        float sy = (worldP.y - camCenter2.y) / camSize2.y * H + H / 2.0f;
                        if (sx < -50.0f || sx > W + 50.0f ||
                            sy < -50.0f || sy > H + 50.0f) continue;

                        if (nuc.type == 0) {
                            nucLabel.setString("P");
                            nucLabel.setFillColor(sf::Color(255, 235, 235, alpha));
                        }
                        else {
                            nucLabel.setString("n");
                            nucLabel.setFillColor(sf::Color(40, 40, 50, alpha));
                        }
                        sf::FloatRect lb = nucLabel.getLocalBounds();
                        nucLabel.setOrigin({ lb.position.x + lb.size.x / 2.0f,
                                              lb.position.y + lb.size.y / 2.0f });
                        nucLabel.setPosition({ sx, sy });
                        target.draw(nucLabel);
                    }
                }
            }
        }

        // ============================================================
        // Заряды атомов (Charges)
        // ============================================================
        if (fontLoaded && ui.showCharges && !atoms.empty()) {
            sf::Text chargeLabel(font, "+", 12);
            chargeLabel.setStyle(sf::Text::Bold);

            sf::Vector2f camCenter2 = camera.getCenter();
            sf::Vector2f camSize2 = camera.getSize();
            float W = (float)winSize.x;
            float H = (float)winSize.y;

            for (const auto& a : atoms) {
                std::string ch = atomChargeLabel(a.elementId);
                if (ch.empty()) continue;

                float wx = a.pos.x + a.radius * 0.85f;
                float wy = a.pos.y - a.radius * 0.85f;
                float sx = (wx - camCenter2.x) / camSize2.x * W + W / 2.0f;
                float sy = (wy - camCenter2.y) / camSize2.y * H + H / 2.0f;
                if (sx < -20.0f || sx > W + 20.0f ||
                    sy < -20.0f || sy > H + 20.0f) continue;

                float screenRadius = a.radius / camSize2.x * W;
                int csize = std::max(8, (int)std::round(screenRadius * 0.9f));
                chargeLabel.setCharacterSize(csize);
                chargeLabel.setString(ch);
                chargeLabel.setFillColor(atomChargeColor(a.elementId));

                sf::FloatRect lb = chargeLabel.getLocalBounds();
                chargeLabel.setOrigin({ lb.position.x + lb.size.x / 2.0f,
                                         lb.position.y + lb.size.y / 2.0f });
                chargeLabel.setPosition({ sx, sy });
                target.draw(chargeLabel);
            }
        }

        const float trackY = PANEL_HEIGHT / 2.0f;
        const float trackRight = SLIDER_LEFT + SLIDER_WIDTH;

        // ============================================================
        // Верхняя панель
        // ============================================================
        {
            sf::RectangleShape panelBg(sf::Vector2f((float)winSize.x, PANEL_HEIGHT));
            panelBg.setFillColor(sf::Color(20, 20, 25, 235));
            panelBg.setOutlineColor(sf::Color(60, 60, 60));
            panelBg.setOutlineThickness(1.0f);
            target.draw(panelBg);

            // В L1 температура заблокирована всегда.
            // В L2 — до наступления фазы 4 (после «воды + пероксид»).
            const bool tempDisabled =
                ui.campaignMode &&
                (ui.campaignLevel == 1
                    || (ui.campaignLevel == 2 && ui.taskPhase < 3));

            if (fontLoaded) {
                sf::Text speedLabel(font, tr(Loc::Speed, ui.language), 14);
                speedLabel.setFillColor(sf::Color(180, 180, 180));
                sf::FloatRect lb = speedLabel.getLocalBounds();
                speedLabel.setOrigin({ lb.position.x, lb.position.y + lb.size.y / 2.0f });
                speedLabel.setPosition({ 16.0f, trackY });
                target.draw(speedLabel);
            }

            sf::RectangleShape track(sf::Vector2f(SLIDER_WIDTH, 4.0f));
            track.setOrigin({ 0.0f, 2.0f });
            track.setPosition({ SLIDER_LEFT, trackY });
            track.setFillColor(sf::Color(70, 70, 80));
            target.draw(track);

            float knobPos = valueToPos(ui.speedSlider.value);
            sf::RectangleShape trackFill(sf::Vector2f(SLIDER_WIDTH * knobPos, 4.0f));
            trackFill.setOrigin({ 0.0f, 2.0f });
            trackFill.setPosition({ SLIDER_LEFT, trackY });
            trackFill.setFillColor(sf::Color(70, 130, 210));
            target.draw(trackFill);

            if (fontLoaded) {
                sf::Text minLbl(font, "0x", 10);
                minLbl.setFillColor(sf::Color(110, 110, 110));
                minLbl.setPosition({ SLIDER_LEFT - 4.0f, trackY + 6.0f });
                target.draw(minLbl);

                sf::Text maxLbl(font, "50x", 10);
                maxLbl.setFillColor(sf::Color(110, 110, 110));
                maxLbl.setPosition({ SLIDER_LEFT + SLIDER_WIDTH - 18.0f, trackY + 6.0f });
                target.draw(maxLbl);
            }

            float knobX = SLIDER_LEFT + knobPos * SLIDER_WIDTH;
            sf::CircleShape knob(SLIDER_KNOB_R);
            knob.setOrigin({ SLIDER_KNOB_R, SLIDER_KNOB_R });
            knob.setPosition({ knobX, trackY });
            bool paused = (ui.speedSlider.value <= 0.0f);
            knob.setFillColor(paused ? sf::Color(240, 160, 60) : sf::Color(90, 160, 240));
            knob.setOutlineColor(sf::Color(200, 220, 255));
            knob.setOutlineThickness(2.0f);
            target.draw(knob);

            if (fontLoaded) {
                sf::Text valueText(font, formatSpeed(ui.speedSlider.value), 15);
                valueText.setFillColor(paused ? sf::Color(255, 200, 100) : sf::Color(220, 220, 220));
                valueText.setPosition({ SLIDER_LEFT + SLIDER_WIDTH + 16.0f, trackY - 9.0f });
                target.draw(valueText);

                float rbX = SLIDER_LEFT + SLIDER_WIDTH + RESET_BTN_GAP;
                float rbY = trackY - RESET_BTN_H / 2.0f;
                sf::RectangleShape resetBtn(sf::Vector2f(RESET_BTN_W, RESET_BTN_H));
                resetBtn.setPosition({ rbX, rbY });
                resetBtn.setFillColor(sf::Color(45, 45, 50));
                resetBtn.setOutlineColor(sf::Color(110, 110, 110));
                resetBtn.setOutlineThickness(1.0f);
                target.draw(resetBtn);

                sf::Text resetLbl(font, "1x", 14);
                sf::FloatRect rb = resetLbl.getLocalBounds();
                resetLbl.setOrigin({ rb.position.x + rb.size.x / 2.0f, rb.position.y + rb.size.y / 2.0f });
                resetLbl.setPosition({ rbX + RESET_BTN_W / 2.0f, rbY + RESET_BTN_H / 2.0f });
                resetLbl.setFillColor(sf::Color(220, 220, 220));
                target.draw(resetLbl);
            }

            // Temp — серый в кампании (уровень 1)
            if (fontLoaded) {
                sf::Text tempLabel(font, tr(Loc::Temp, ui.language), 14);
                tempLabel.setFillColor(tempDisabled
                    ? sf::Color(90, 90, 95)
                    : sf::Color(180, 180, 180));
                sf::FloatRect tl = tempLabel.getLocalBounds();
                tempLabel.setOrigin({ tl.position.x, tl.position.y + tl.size.y / 2.0f });
                tempLabel.setPosition({ TEMP_LABEL_X, trackY });
                target.draw(tempLabel);
            }

            sf::RectangleShape tTrack(sf::Vector2f(TEMP_SLIDER_WIDTH, 4.0f));
            tTrack.setOrigin({ 0.0f, 2.0f });
            tTrack.setPosition({ TEMP_SLIDER_LEFT, trackY });
            tTrack.setFillColor(tempDisabled
                ? sf::Color(50, 50, 55)
                : sf::Color(70, 70, 80));
            target.draw(tTrack);

            float tKnobPos = tempCelsiusToSliderPos(ui.targetTempCelsius);
            sf::RectangleShape tTrackFill(sf::Vector2f(TEMP_SLIDER_WIDTH * tKnobPos, 4.0f));
            tTrackFill.setOrigin({ 0.0f, 2.0f });
            tTrackFill.setPosition({ TEMP_SLIDER_LEFT, trackY });
            tTrackFill.setFillColor(tempDisabled
                ? sf::Color(70, 70, 75)
                : sf::Color(210, 110, 60));
            target.draw(tTrackFill);

            if (fontLoaded) {
                sf::Text tMinLbl(font, "-273", 9);
                tMinLbl.setFillColor(tempDisabled
                    ? sf::Color(70, 70, 75)
                    : sf::Color(110, 110, 110));
                tMinLbl.setPosition({ TEMP_SLIDER_LEFT - 6.0f, trackY + 6.0f });
                target.draw(tMinLbl);

                sf::Text tMaxLbl(font, "110", 9);
                tMaxLbl.setFillColor(tempDisabled
                    ? sf::Color(70, 70, 75)
                    : sf::Color(110, 110, 110));
                tMaxLbl.setPosition({ TEMP_SLIDER_LEFT + TEMP_SLIDER_WIDTH - 12.0f, trackY + 6.0f });
                target.draw(tMaxLbl);
            }

            float tKnobX = TEMP_SLIDER_LEFT + tKnobPos * TEMP_SLIDER_WIDTH;
            sf::CircleShape tKnob(SLIDER_KNOB_R);
            tKnob.setOrigin({ SLIDER_KNOB_R, SLIDER_KNOB_R });
            tKnob.setPosition({ tKnobX, trackY });
            bool tempIsZero = (ui.targetTempCelsius <= TEMP_MIN_C + 0.01f);
            if (tempDisabled) {
                tKnob.setFillColor(sf::Color(70, 70, 75));
                tKnob.setOutlineColor(sf::Color(120, 120, 125));
            }
            else {
                tKnob.setFillColor(tempIsZero ? sf::Color(120, 120, 140) : sf::Color(230, 130, 70));
                tKnob.setOutlineColor(sf::Color(255, 220, 200));
            }
            tKnob.setOutlineThickness(2.0f);
            target.draw(tKnob);

            sf::RectangleShape tField(sf::Vector2f(TEMP_FIELD_W, TEMP_FIELD_H));
            tField.setPosition({ TEMP_FIELD_LEFT, trackY - TEMP_FIELD_H / 2.0f });
            if (tempDisabled) {
                tField.setFillColor(sf::Color(22, 22, 26));
                tField.setOutlineColor(sf::Color(70, 70, 75));
                tField.setOutlineThickness(1.0f);
            }
            else {
                tField.setFillColor(ui.tempEditing ? sf::Color(30, 50, 75) : sf::Color(25, 25, 30));
                tField.setOutlineColor(ui.tempEditing ? sf::Color(255, 160, 100) : sf::Color(90, 90, 100));
                tField.setOutlineThickness(ui.tempEditing ? 2.0f : 1.0f);
            }
            target.draw(tField);

            if (fontLoaded) {
                std::string displayStr;
                if (ui.tempEditing && !tempDisabled) {
                    displayStr = ui.tempEditBuffer;
                    float t = ui.cursorClock.getElapsedTime().asSeconds();
                    bool cursorOn = (std::fmod(t, 1.0f) < 0.5f);
                    if (cursorOn) displayStr += "|";
                }
                else {
                    displayStr = formatTempCelsius(ui.targetTempCelsius);
                }

                sf::Text tValueText(font, displayStr, 14);
                sf::FloatRect tcb = tValueText.getLocalBounds();
                tValueText.setOrigin({ tcb.position.x + tcb.size.x / 2.0f, tcb.position.y + tcb.size.y / 2.0f });
                tValueText.setPosition({ TEMP_FIELD_LEFT + TEMP_FIELD_W / 2.0f, trackY });
                if (tempDisabled)
                    tValueText.setFillColor(sf::Color(120, 120, 125));
                else
                    tValueText.setFillColor(ui.tempEditing ? sf::Color(255, 255, 255) : sf::Color(255, 210, 170));
                target.draw(tValueText);
            }

            sf::RectangleShape tResetBtn(sf::Vector2f(TEMP_RESET_W, TEMP_RESET_H));
            tResetBtn.setPosition({ TEMP_RESET_LEFT, trackY - TEMP_RESET_H / 2.0f });
            tResetBtn.setFillColor(tempDisabled
                ? sf::Color(35, 35, 40)
                : sf::Color(45, 45, 50));
            tResetBtn.setOutlineColor(tempDisabled
                ? sf::Color(70, 70, 75)
                : sf::Color(110, 110, 110));
            tResetBtn.setOutlineThickness(1.0f);
            target.draw(tResetBtn);

            if (fontLoaded) {
                sf::Text tResetLbl(font, "0", 14);
                sf::FloatRect tRb = tResetLbl.getLocalBounds();
                tResetLbl.setOrigin({ tRb.position.x + tRb.size.x / 2.0f, tRb.position.y + tRb.size.y / 2.0f });
                tResetLbl.setPosition({ TEMP_RESET_LEFT + TEMP_RESET_W / 2.0f, trackY });
                tResetLbl.setFillColor(tempDisabled
                    ? sf::Color(120, 120, 125)
                    : sf::Color(220, 220, 220));
                target.draw(tResetLbl);
            }
        }
        // ============================================================
        // Кнопка "Charges"
        // ============================================================
        if (!ui.campaignMode) {
            float cbX = CHARGE_BTN_LEFT;
            float cbY = trackY - CHARGE_BTN_H / 2.0f;

            sf::RectangleShape chargeBtn(sf::Vector2f(CHARGE_BTN_W, CHARGE_BTN_H));
            chargeBtn.setPosition({ cbX, cbY });
            chargeBtn.setFillColor(ui.showCharges
                ? sf::Color(70, 130, 210)
                : sf::Color(45, 45, 50));
            chargeBtn.setOutlineColor(ui.showCharges
                ? sf::Color(150, 190, 255)
                : sf::Color(110, 110, 110));
            chargeBtn.setOutlineThickness(1.0f);
            target.draw(chargeBtn);

            if (fontLoaded) {
                sf::Text chargeLbl(font, tr(Loc::Charges, ui.language), 14);
                sf::FloatRect cb = chargeLbl.getLocalBounds();
                chargeLbl.setOrigin({ cb.position.x + cb.size.x / 2.0f,
                                       cb.position.y + cb.size.y / 2.0f });
                chargeLbl.setPosition({ cbX + CHARGE_BTN_W / 2.0f,
                                         cbY + CHARGE_BTN_H / 2.0f });
                chargeLbl.setFillColor(ui.showCharges
                    ? sf::Color::White
                    : sf::Color(200, 200, 200));
                target.draw(chargeLbl);
            }
        }

        // ============================================================
        // Панель задач (кампания)
        // ============================================================
        if (ui.campaignMode && fontLoaded
            && (ui.campaignLevel == 1 || ui.campaignLevel == 2
                || ui.campaignLevel == 3)) {
            const bool isLevel2 = (ui.campaignLevel == 2);
            const bool isLevel3 = (ui.campaignLevel == 3);
            const int  finalPhase = isLevel3 ? 3 : (isLevel2 ? 5 : 3);

            float tW = 360.0f * S;
            float tMargin = STATS_MARGIN * S;
            float tHeaderH = MENU_HEADER_H * S;
            float tH = (isLevel3 ? 460.0f : (isLevel2 ? 480.0f : 400.0f)) * S;

            float tX = (float)winSize.x - tW - tMargin;
            float tY = PANEL_HEIGHT + 10.0f * S;

            // Рамка панели; при завершении — зелёная
            sf::RectangleShape bg(sf::Vector2f(tW, tH));
            bg.setPosition({ tX, tY });
            bg.setFillColor(sf::Color(25, 25, 30, 240));
            bg.setOutlineColor(ui.taskPhase == finalPhase
                ? sf::Color(90, 200, 130)
                : sf::Color(70, 70, 80));
            bg.setOutlineThickness(ui.taskPhase == finalPhase ? 2.0f : 1.0f);
            target.draw(bg);

            sf::RectangleShape header(sf::Vector2f(tW, tHeaderH));
            header.setPosition({ tX, tY });
            header.setFillColor(sf::Color(40, 40, 50, 255));
            target.draw(header);

            {
                int ts = (int)std::round(14.0f * S);
                sf::Text t(font, tr(Loc::TaskHeader, ui.language), ts);
                t.setFillColor(sf::Color(220, 220, 220));
                t.setStyle(sf::Text::Bold);
                t.setPosition({ tX + 10.0f * S, tY + 5.0f * S });
                target.draw(t);
            }

            float a = std::clamp(ui.taskTextAlpha, 0.0f, 1.0f);
            std::uint8_t a255 = static_cast<std::uint8_t>(a * 255.0f);

            float y = tY + tHeaderH + 10.0f * S;

            // --- Название уровня ---
            {
                int ts = (int)std::round(16.0f * S);
                sf::String titleText;
                if (isLevel3) titleText = tr(Loc::Level3Title, ui.language);
                else if (isLevel2) titleText = tr(Loc::Level2Title, ui.language);
                else titleText = tr(Loc::Level1Title, ui.language);
                sf::Text t(font, titleText, ts);
                t.setFillColor(sf::Color(255, 220, 150, a255));
                t.setStyle(sf::Text::Bold);
                t.setPosition({ tX + 12.0f * S, y });
                target.draw(t);
            }
            y += 24.0f * S;

            // --- Цель (разная в разных фазах) ---
            {
                int ts = (int)std::round(13.0f * S);
                sf::String goalText;
                if (isLevel3) {
                    if (ui.taskPhase == 0)      goalText = tr(Loc::Goal3Phase0, ui.language);
                    else if (ui.taskPhase == 1) goalText = tr(Loc::Goal3Phase1, ui.language);
                    else if (ui.taskPhase == 2) goalText = tr(Loc::Goal3Phase2, ui.language);
                    else                        goalText = tr(Loc::Goal3Complete, ui.language);
                }
                else if (!isLevel2) {
                    if (ui.taskPhase == 1) goalText = tr(Loc::GoalPhase1, ui.language);
                    else if (ui.taskPhase == 2) goalText = tr(Loc::GoalPhase2, ui.language);
                    else                        goalText = tr(Loc::GoalComplete, ui.language);
                }
                else {
                    if (ui.taskPhase == 1) goalText = tr(Loc::Goal2Phase1, ui.language);
                    else if (ui.taskPhase == 2) goalText = tr(Loc::Goal2Phase2, ui.language);
                    else if (ui.taskPhase == 3) goalText = tr(Loc::Goal2Phase3, ui.language);
                    else if (ui.taskPhase == 4) goalText = tr(Loc::Goal2Phase4, ui.language);
                    else                        goalText = tr(Loc::Goal2Complete, ui.language);
                }
                sf::Text t(font, goalText, ts);
                t.setFillColor(sf::Color(200, 210, 225, a255));
                t.setPosition({ tX + 12.0f * S, y });
                target.draw(t);
                // Учитываем реальную высоту текста: если цель занимает
                // две строки (после \n), прогресс-бар сдвинется ниже,
                // а не налезет на «одновременно».
                float goalH = t.getLocalBounds().size.y;
                if (goalH < 16.0f * S) goalH = 16.0f * S;   // минимум
                y += goalH + 6.0f * S;
            }

            // --- Прогресс-бар ---
            {
                float barX = tX + 12.0f * S;
                float barY = y;
                float barW = tW - 24.0f * S;
                float barH = 22.0f * S;

                sf::RectangleShape barBg({ barW, barH });
                barBg.setPosition({ barX, barY });
                barBg.setFillColor(sf::Color(20, 20, 25, a255));
                barBg.setOutlineColor(sf::Color(90, 90, 100, a255));
                barBg.setOutlineThickness(1.0f);
                target.draw(barBg);

                // Считаем прогресс и подпись в зависимости от уровня/фазы
                float frac = 0.0f;
                sf::String barLabel;

                auto appendCount = [](sf::String& s, const sf::String& pre,
                    int cnt, int target) {
                        s += pre + sf::String(" ") + sf::String(std::to_string(cnt))
                            + sf::String(" / ") + sf::String(std::to_string(target));
                    };

                if (isLevel3) {
                    if (ui.taskPhase == 0) {
                        frac = std::clamp((float)ui.taskHClCount / 3.0f,
                            0.0f, 1.0f);
                        appendCount(barLabel, tr(Loc::TaskHClPrefix, ui.language),
                            ui.taskHClCount, 3);
                    }
                    else if (ui.taskPhase == 1) {
                        frac = std::clamp((float)ui.taskH3OCount / 3.0f, 0.0f, 1.0f);
                        appendCount(barLabel, tr(Loc::TaskH3OPrefix, ui.language),
                            ui.taskH3OCount, 3);
                    }
                    else {
                        // Фаза 2 (задача) и фаза 3 (complete) — держим счётчик Cl
                        frac = std::clamp((float)ui.taskSingleClCount / 10.0f,
                            0.0f, 1.0f);
                        appendCount(barLabel, tr(Loc::TaskClPrefix, ui.language),
                            ui.taskSingleClCount, 10);
                    }
                }
                else if (!isLevel2) {
                    if (ui.taskH2Target > 0)
                        frac = std::clamp((float)ui.taskH2Count / (float)ui.taskH2Target, 0.0f, 1.0f);
                    appendCount(barLabel, tr(Loc::TaskH2Prefix, ui.language),
                        ui.taskH2Count, ui.taskH2Target);
                }
                else {
                    if (ui.taskPhase == 1) {
                        frac = std::clamp((float)ui.taskH2OCount / 5.0f, 0.0f, 1.0f);
                        appendCount(barLabel, tr(Loc::TaskH2OPrefix, ui.language),
                            ui.taskH2OCount, 5);
                    }
                    else if (ui.taskPhase == 2) {
                        frac = std::clamp((float)ui.taskH2O2Count / 5.0f, 0.0f, 1.0f);
                        appendCount(barLabel, tr(Loc::TaskH2O2Prefix, ui.language),
                            ui.taskH2O2Count, 5);
                    }
                    else if (ui.taskPhase == 3) {
                        float f1 = std::clamp((float)ui.taskH2OCount / 5.0f, 0.0f, 1.0f);
                        float f2 = std::clamp((float)ui.taskH2O2Count / 5.0f, 0.0f, 1.0f);
                        frac = std::min(f1, f2);
                        appendCount(barLabel, tr(Loc::TaskH2OPrefix, ui.language),
                            ui.taskH2OCount, 5);
                        barLabel += sf::String("   ");
                        appendCount(barLabel, tr(Loc::TaskH2O2Prefix, ui.language),
                            ui.taskH2O2Count, 5);
                    }
                    else {
                        // Фаза 4 (задача) и фаза 5 (complete) — держим счётчик H-связей
                        frac = std::clamp((float)ui.taskHBondCount / 8.0f, 0.0f, 1.0f);
                        appendCount(barLabel, tr(Loc::TaskHBondPrefix, ui.language),
                            ui.taskHBondCount, 8);
                    }
                }

                if (frac > 0.0f) {
                    sf::RectangleShape fill({ barW * frac, barH });
                    fill.setPosition({ barX, barY });
                    sf::Color fc = (ui.taskPhase == finalPhase)
                        ? sf::Color(70, 180, 110, a255)
                        : sf::Color(70, 130, 210, a255);
                    fill.setFillColor(fc);
                    target.draw(fill);
                }

                int fs = (int)std::round(13.0f * S);
                sf::Text t(font, barLabel, fs);
                t.setFillColor(sf::Color(255, 255, 255, a255));
                t.setStyle(sf::Text::Bold);
                sf::FloatRect tb = t.getLocalBounds();
                t.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                              tb.position.y + tb.size.y * 0.5f });
                t.setPosition({ barX + barW * 0.5f, barY + barH * 0.5f });
                target.draw(t);
            }
            y += 22.0f * S + 12.0f * S;

            // --- Таймер-бар удержания Cl (L3 фаза 2) ---
            if (isLevel3 && ui.taskPhase == 2) {
                float barX = tX + 12.0f * S;
                float barY = y;
                float barW = tW - 24.0f * S;
                float barH = 22.0f * S;

                float timerFrac = std::clamp(ui.taskClHoldTimer / 30.0f,
                    0.0f, 1.0f);

                sf::RectangleShape barBg({ barW, barH });
                barBg.setPosition({ barX, barY });
                barBg.setFillColor(sf::Color(20, 20, 25, a255));
                barBg.setOutlineColor(sf::Color(90, 90, 100, a255));
                barBg.setOutlineThickness(1.0f);
                target.draw(barBg);

                if (timerFrac > 0.0f) {
                    sf::RectangleShape fill({ barW * timerFrac, barH });
                    fill.setPosition({ barX, barY });
                    sf::Color tc = (ui.taskClFlashTimer > 0.0f)
                        ? sf::Color(220, 70, 70, a255)
                        : sf::Color(70, 180, 110, a255);
                    fill.setFillColor(tc);
                    target.draw(fill);
                }

                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.1f / 30.0 s",
                    ui.taskClHoldTimer);
                int fs = (int)std::round(13.0f * S);
                sf::Text t(font, buf, fs);
                t.setFillColor(sf::Color(255, 255, 255, a255));
                t.setStyle(sf::Text::Bold);
                sf::FloatRect tb = t.getLocalBounds();
                t.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                              tb.position.y + tb.size.y * 0.5f });
                t.setPosition({ barX + barW * 0.5f, barY + barH * 0.5f });
                target.draw(t);

                y += 22.0f * S + 8.0f * S;
            }

            // --- Кнопка Hint ---
            //  • L2 фаза 2 (пероксид) и фаза 4 (лёд)
            //  • L3 фазы 1 (H3O+), 2 (одиночные Cl), 3 (complete)
            const bool hintAvailable =
                (isLevel2 && (ui.taskPhase == 2 || ui.taskPhase == 4))
                || (isLevel3 && (ui.taskPhase >= 1 && ui.taskPhase <= 3));
            if (hintAvailable) {
                sf::FloatRect hbRect(
                    { tX + 12.0f * S, y },
                    { tW - 24.0f * S, 22.0f * S });

                // Запоминаем прямоугольник для hit-теста в AtomSimulation.cpp.
                // Y зависит от фактической высоты цели (одна строка / две),
                // поэтому нельзя вычислить его «на месте» снаружи.
                ui.hintButtonRect = hbRect;

                sf::Vector2f mpF((float)mousePixel.x, (float)mousePixel.y);
                bool hbHover = hbRect.contains(mpF);

                sf::RectangleShape hb({ hbRect.size.x, hbRect.size.y });
                hb.setPosition(hbRect.position);
                hb.setFillColor(ui.hintButtonOpen
                    ? sf::Color(60, 110, 180, 230)
                    : (hbHover ? sf::Color(70, 130, 210, 230)
                        : sf::Color(45, 45, 55, 220)));
                hb.setOutlineColor(sf::Color(90, 120, 170));
                hb.setOutlineThickness(1.0f);
                target.draw(hb);

                int hs = (int)std::round(13.0f * S);
                sf::String hbLabel = ui.hintButtonOpen
                    ? tr(Loc::HideHint, ui.language)
                    : tr(Loc::ShowHint, ui.language);
                sf::Text ht(font, hbLabel, hs);
                ht.setFillColor(sf::Color(230, 235, 245));
                ht.setStyle(sf::Text::Bold);
                sf::FloatRect htb = ht.getLocalBounds();
                ht.setOrigin({ htb.position.x + htb.size.x * 0.5f,
                               htb.position.y + htb.size.y * 0.5f });
                ht.setPosition({ hbRect.position.x + hbRect.size.x * 0.5f,
                                 hbRect.position.y + hbRect.size.y * 0.5f });
                target.draw(ht);

                y += 22.0f * S + 8.0f * S;
            }

            // --- Разделитель ---
            {
                sf::RectangleShape line(sf::Vector2f(tW - 24.0f * S, 1.0f));
                line.setPosition({ tX + 12.0f * S, y });
                line.setFillColor(sf::Color(60, 60, 70, a255));
                target.draw(line);
            }
            y += 8.0f * S;

            // --- Подсказки ---
            int hfs = (int)std::round(12.0f * S);
            float hRowH = 17.0f * S;

            auto drawHint = [&](const sf::String& s) {
                sf::Text t(font, s, hfs);
                t.setFillColor(sf::Color(170, 180, 200, a255));
                t.setPosition({ tX + 12.0f * S, y });
                target.draw(t);
                y += hRowH;
                };

            if (isLevel3) {
                if (ui.taskPhase == 1 && ui.hintButtonOpen) {
                    drawHint(tr(Loc::Hint3_1_1, ui.language));
                    drawHint(tr(Loc::Hint3_1_2, ui.language));
                    drawHint(tr(Loc::Hint3_1_3, ui.language));
                    drawHint(tr(Loc::Hint3_1_4, ui.language));
                    drawHint(tr(Loc::Hint3_1_5, ui.language));
                }
                else if (ui.taskPhase == 2 && ui.hintButtonOpen) {
                    drawHint(tr(Loc::Hint3_2_1, ui.language));
                    drawHint(tr(Loc::Hint3_2_2, ui.language));
                    drawHint(tr(Loc::Hint3_2_3, ui.language));
                }
                else if (ui.taskPhase == 3 && ui.hintButtonOpen) {
                    drawHint(tr(Loc::Hint3_1, ui.language));
                    drawHint(tr(Loc::Level3CompleteDesc, ui.language));
                }
                // Фаза 0 — без подсказок.
            }
            else if (!isLevel2) {
                if (ui.taskPhase == 1) {
                    drawHint(tr(Loc::HowToPlay, ui.language));
                    drawHint(tr(Loc::Hint1_1, ui.language));
                    drawHint(tr(Loc::Hint1_2, ui.language));
                    drawHint(tr(Loc::Hint1_3, ui.language));
                    drawHint(tr(Loc::Hint1_4, ui.language));
                    drawHint(tr(Loc::Hint1_5, ui.language));
                    drawHint(tr(Loc::Hint1_6, ui.language));
                }
                else if (ui.taskPhase == 2) {
                    drawHint(tr(Loc::Hint2_1, ui.language));
                    drawHint(tr(Loc::Hint2_2, ui.language));
                    drawHint(tr(Loc::Hint2_3, ui.language));
                    drawHint(tr(Loc::Hint2_4, ui.language));
                    y += 2.0f * S;
                    drawHint(tr(Loc::Hint2_5, ui.language));
                    drawHint(tr(Loc::Hint2_6, ui.language));
                }
                else {
                    drawHint(tr(Loc::Hint3_1, ui.language));
                    drawHint(tr(Loc::Hint3_2, ui.language));
                }
            }
            else {
                // L2
                if (ui.taskPhase == 1) {
                    // Сообщаем про RMB-меню (в две строки, чтобы
                    // влезло в панель)
                    drawHint(tr(Loc::HintL2_1_5, ui.language));
                    drawHint(tr(Loc::HintL2_1_6, ui.language));
                }
                else if (ui.taskPhase == 2 && ui.hintButtonOpen) {
                    drawHint(tr(Loc::HintL2_2_1, ui.language));
                    drawHint(tr(Loc::HintL2_2_2, ui.language));
                    drawHint(tr(Loc::HintL2_2_3, ui.language));
                    y += 2.0f * S;
                    drawHint(tr(Loc::HintL2_2_4, ui.language));
                    drawHint(tr(Loc::HintL2_2_5, ui.language));
                }
                else if (ui.taskPhase == 3) {
                    // Сообщаем про разблокировку температуры
                    drawHint(tr(Loc::HintL2_3_1, ui.language));
                }
                else if (ui.taskPhase == 4 && ui.hintButtonOpen) {
                    drawHint(tr(Loc::HintL2_4_2, ui.language));
                    drawHint(tr(Loc::HintL2_4_3, ui.language));
                    drawHint(tr(Loc::HintL2_4_4, ui.language));
                }
                else if (ui.taskPhase == 5) {
                    drawHint(tr(Loc::Hint3_1, ui.language));
                    drawHint(tr(Loc::Level2CompleteDesc, ui.language));
                }
            }

            y += 6.0f * S;

            // --- Финальный статус + кнопка ---
            if (ui.taskPhase == finalPhase) {
                // На всех уровнях пишем "Уровень пройден!"
                int ss = (int)std::round(14.0f * S);
                sf::Text t(font, tr(Loc::LevelComplete, ui.language), ss);
                t.setFillColor(sf::Color(120, 220, 150, a255));
                t.setStyle(sf::Text::Bold);
                t.setPosition({ tX + 12.0f * S, y });
                target.draw(t);
                y += 22.0f * S;

                // Жёлтый курсив
                if (!isLevel2 && !isLevel3) {
                    int fs = (int)std::round(12.0f * S);
                    sf::Text t2(font, tr(Loc::CompressQuote, ui.language), fs);
                    t2.setFillColor(sf::Color(230, 220, 170, a255));
                    t2.setStyle(sf::Text::Italic);
                    t2.setPosition({ tX + 12.0f * S, y });
                    target.draw(t2);
                }
                else if (isLevel2) {
                    int fs = (int)std::round(12.0f * S);
                    sf::Text t2(font, tr(Loc::PeroxideQuote, ui.language), fs);
                    t2.setFillColor(sf::Color(230, 220, 170, a255));
                    t2.setStyle(sf::Text::Italic);
                    t2.setPosition({ tX + 12.0f * S, y });
                    target.draw(t2);
                }

                sf::FloatRect nr = nextLevelButtonRect(winSize,
                    isLevel3 ? 460.0f : (isLevel2 ? 480.0f : 400.0f));

                sf::RectangleShape nbg({ nr.size.x, nr.size.y });
                nbg.setPosition(nr.position);
                if (ui.nextLevelButtonHovered) {
                    nbg.setFillColor(sf::Color(90, 180, 130, 240));
                    nbg.setOutlineColor(sf::Color(180, 255, 200, 255));
                }
                else {
                    nbg.setFillColor(sf::Color(50, 130, 90, 230));
                    nbg.setOutlineColor(sf::Color(140, 220, 170, 255));
                }
                nbg.setOutlineThickness(1.5f * S);
                target.draw(nbg);

                int nfs = (int)std::round(15.0f * S);
                sf::String nl;
                if (isLevel3) nl = tr(Loc::FinishLevel, ui.language);
                else          nl = tr(Loc::NextLevel, ui.language);
                sf::Text nt(font, nl, nfs);
                nt.setFillColor(sf::Color(230, 245, 235, a255));
                nt.setStyle(sf::Text::Bold);
                sf::FloatRect ntb = nt.getLocalBounds();
                nt.setOrigin({ ntb.position.x + ntb.size.x * 0.5f,
                               ntb.position.y + ntb.size.y * 0.5f });
                nt.setPosition({ nr.position.x + nr.size.x * 0.5f,
                                 nr.position.y + nr.size.y * 0.5f });
                target.draw(nt);
            }
        }

        // ============================================================
        // Панель статистики
        // ============================================================
        if (fontLoaded && ui.focusedAtomIndex >= 0 && ui.focusedAtomIndex < (int)atoms.size()) {
            const Atom& a = atoms[ui.focusedAtomIndex];

            float sW = STATS_W * S;
            float sMargin = STATS_MARGIN * S;
            float headerH = MENU_HEADER_H * S;
            float statsH = 296.0f * S;

            float statsX = (float)winSize.x - sW - sMargin;
            float statsY = PANEL_HEIGHT + 10.0f;

            // В кампании панель задач занимает верх, поэтому статистику
    // сдвигаем вниз под неё.
            if (ui.campaignMode && ui.campaignLevel == 1) {
                statsY += 400.0f * S + 10.0f * S;
            }
            else if (ui.campaignMode && ui.campaignLevel == 2) {
                statsY += 480.0f * S + 10.0f * S;
            }
            else if (ui.campaignMode && ui.campaignLevel == 3) {
                statsY += 460.0f * S + 10.0f * S;
            }

            sf::RectangleShape bg(sf::Vector2f(sW, statsH));
            bg.setPosition({ statsX, statsY });
            bg.setFillColor(sf::Color(25, 25, 30, 240));
            bg.setOutlineColor(sf::Color(70, 70, 80));
            bg.setOutlineThickness(1.0f);
            target.draw(bg);

            sf::RectangleShape header(sf::Vector2f(sW, headerH));
            header.setPosition({ statsX, statsY });
            header.setFillColor(sf::Color(40, 40, 50, 255));
            target.draw(header);

            {
                int titleSize = (int)std::round(14.0f * S);
                // FIX: snprintf с tr() не компилируется — собираем sf::String
                sf::String titleStr = tr(Loc::StatsAtom, ui.language);
                titleStr += sf::String(" #")
                    + sf::String(std::to_string(ui.focusedAtomIndex));
                sf::Text title(font, titleStr, titleSize);
                title.setFillColor(sf::Color(220, 220, 220));
                title.setPosition({ statsX + 10.0f * S, statsY + 5.0f * S });
                target.draw(title);
            }

            float y = statsY + headerH + 10.0f * S;

            {
                float iconR = 14.0f * S;
                float iconX = statsX + 20.0f * S + iconR;
                float iconY = y + iconR;

                sf::CircleShape icon(iconR);
                icon.setOrigin({ iconR, iconR });
                icon.setPosition({ iconX, iconY });
                icon.setFillColor(a.color);
                icon.setOutlineColor(atomOutlineColor(a.elementId));
                icon.setOutlineThickness(1.5f);
                target.draw(icon);

                int hSize = (int)std::round(14.0f * S);
                sf::Text hLbl(font, ATOM_TYPES[a.elementId].symbol, hSize);
                hLbl.setStyle(sf::Text::Bold);
                hLbl.setFillColor(sf::Color(30, 30, 30));
                sf::FloatRect lb = hLbl.getLocalBounds();
                hLbl.setOrigin({ lb.position.x + lb.size.x / 2.0f, lb.position.y + lb.size.y / 2.0f });
                hLbl.setPosition({ iconX, iconY });
                target.draw(hLbl);

                int nameSize = (int)std::round(15.0f * S);
                sf::Text name(font, locAtomName(a.elementId, ui.language), nameSize);
                name.setFillColor(sf::Color(220, 220, 220));
                name.setPosition({ statsX + 20.0f * S + iconR * 2.0f + 12.0f * S, y + 2.0f * S });
                target.draw(name);

                int symSize = (int)std::round(12.0f * S);
                sf::Text sym(font, "(" + ATOM_TYPES[a.elementId].symbol + ")", symSize);
                sym.setFillColor(sf::Color(150, 150, 160));
                sym.setPosition({ statsX + 20.0f * S + iconR * 2.0f + 12.0f * S, y + 20.0f * S });
                target.draw(sym);
            }

            y += 44.0f * S;

            {
                sf::RectangleShape line(sf::Vector2f(sW - 20.0f * S, 1.0f));
                line.setPosition({ statsX + 10.0f * S, y });
                line.setFillColor(sf::Color(60, 60, 70));
                target.draw(line);
            }
            y += 8.0f * S;

            int labelSize = (int)std::round(12.0f * S);
            float rowH = 18.0f * S;

            // FIX: параметр — sf::String, чтобы принимать tr(...)
            auto drawRow = [&](const sf::String& label, const sf::String& value, float yy) {
                sf::Text l(font, label, labelSize);
                l.setFillColor(sf::Color(150, 150, 160));
                l.setPosition({ statsX + 12.0f * S, yy });
                target.draw(l);

                sf::Text v(font, value, labelSize);
                v.setFillColor(sf::Color(220, 220, 220));
                sf::FloatRect vb = v.getLocalBounds();
                v.setPosition({ statsX + sW - 12.0f * S - vb.size.x, yy });
                target.draw(v);
                };

            { char buf[64]; std::snprintf(buf, sizeof(buf), "%.3f u", a.mass); drawRow(tr(Loc::StatsMass, ui.language), buf, y); y += rowH; }
            { char buf[64]; std::snprintf(buf, sizeof(buf), "%.3f A", a.radius); drawRow(tr(Loc::StatsRadius, ui.language), buf, y); y += rowH; }
            { char buf[64]; std::snprintf(buf, sizeof(buf), "%.2f s", a.age); drawRow(tr(Loc::StatsAge, ui.language), buf, y); y += rowH; }
            {
                float sp = std::sqrt(a.vel.x * a.vel.x + a.vel.y * a.vel.y);
                char buf[64]; std::snprintf(buf, sizeof(buf), "%.3f A/s", sp);
                drawRow(tr(Loc::StatsSpeed, ui.language), buf, y); y += rowH;
            }
            { char buf[64]; std::snprintf(buf, sizeof(buf), "(%.2f, %.2f)", a.pos.x, a.pos.y); drawRow(tr(Loc::StatsPosition, ui.language), buf, y); y += rowH; }
            {
                int n = countBonds(a);
                int maxB = ATOM_TYPES[a.elementId].maxBonds;
                char buf[64]; std::snprintf(buf, sizeof(buf), "%d / %d", n, maxB);
                drawRow(tr(Loc::StatsBonds, ui.language), buf, y); y += rowH;
            }
            {
                int hb = countHBonds(a);
                char buf[64];
                if (a.elementId == 1) {
                    int nc = 0;
                    float q = computeTetrahedralOrder(atoms, ui.focusedAtomIndex, nc);
                    char buf2[64];
                    if (nc < 4) std::snprintf(buf2, sizeof(buf2), "n/a (%d/4)", nc);
                    else        std::snprintf(buf2, sizeof(buf2), "%.3f", q);
                    drawRow(tr(Loc::StatsTetra, ui.language), buf2, y); y += rowH;
                    // FIX: раньше buf не заполнялся на этой ветке →
                    // выводилась неинициализированная память ("iiiiii…").
                    std::snprintf(buf, sizeof(buf), "%d / %d", hb, HBOND_SLOTS_O);
                }
                else if (a.elementId == 2) {
                    std::snprintf(buf, sizeof(buf), "%d / %d", hb, HCL_HB_SLOTS_CL);
                }
                else {
                    std::snprintf(buf, sizeof(buf), "%d / 1", hb);
                }
                drawRow(tr(Loc::StatsHBonds, ui.language), buf, y); y += rowH;
            }
            {
                std::string val = "-";
                for (int k : a.bonds) {
                    if (k >= 0) {
                        if (val != "-") val += ", ";
                        else val = "";
                        val += "#" + std::to_string(k);
                    }
                }
                drawRow(tr(Loc::StatsPartners, ui.language), val, y); y += rowH;
            }
            {
                int selectedCount = 0;
                for (const auto& x : atoms) if (x.selected) selectedCount++;
                // FIX: раньше был trRaw + snprintf → на RU выводился мусор.
                // Теперь drawRow принимает sf::String — передаём tr() напрямую.
                drawRow(tr(Loc::StatsSelected, ui.language),
                    a.selected ? tr(Loc::StatsYes, ui.language)
                    : tr(Loc::StatsNo, ui.language), y);
                y += rowH;

                char buf2[64]; std::snprintf(buf2, sizeof(buf2), "%d", selectedCount);
                drawRow(tr(Loc::StatsSelGroup, ui.language), buf2, y); y += rowH;
            }
        }

        // ============================================================
        // RMB-контекстное меню
        // ============================================================
        const bool rmbMenuAllowed = !ui.campaignMode
            || (ui.campaignMode && ui.campaignLevel >= 2);

        if (rmbMenuAllowed && fontLoaded && ui.rmbMenuOpen) {
            const float RMB_MENU_W = 160.0f;
            const float RMB_ITEM_H = 30.0f;
            const int   RMB_ITEMS = 3;
            const float RMB_MENU_H = RMB_ITEM_H * RMB_ITEMS + 4.0f;

            float mx = (float)ui.rmbMenuPos.x;
            float my = (float)ui.rmbMenuPos.y;
            if (mx + RMB_MENU_W > (float)winSize.x) mx = (float)winSize.x - RMB_MENU_W;
            if (my + RMB_MENU_H > (float)winSize.y) my = (float)winSize.y - RMB_MENU_H;

            sf::RectangleShape bg(sf::Vector2f(RMB_MENU_W, RMB_MENU_H));
            bg.setPosition({ mx, my });
            bg.setFillColor(sf::Color(30, 30, 35, 245));
            bg.setOutlineColor(sf::Color(90, 90, 100));
            bg.setOutlineThickness(1.0f);
            target.draw(bg);

            // FIX: было const char* — tr() теперь sf::String
            sf::String items[RMB_ITEMS] = {
                tr(Loc::RmbCopy, ui.language),
                tr(Loc::RmbDelete, ui.language),
                tr(Loc::RmbDeselect, ui.language)
            };

            sf::Vector2i mp = mousePixel;
            sf::Vector2f mpF((float)mp.x, (float)mp.y);

            for (int i = 0; i < RMB_ITEMS; i++) {
                float iy = my + 2.0f + i * RMB_ITEM_H;
                sf::FloatRect itemRect({ mx + 2.0f, iy }, { RMB_MENU_W - 4.0f, RMB_ITEM_H - 2.0f });
                bool hover = itemRect.contains(mpF);

                sf::RectangleShape item({ RMB_MENU_W - 4.0f, RMB_ITEM_H - 2.0f });
                item.setPosition({ mx + 2.0f, iy });
                item.setFillColor(hover ? sf::Color(70, 130, 210, 200) : sf::Color(45, 45, 55, 200));
                target.draw(item);

                sf::Text t(font, items[i], 14);
                t.setFillColor(sf::Color(230, 230, 230));
                t.setPosition({ mx + 12.0f, iy + 5.0f });
                target.draw(t);
            }
        }

        // ============================================================
        // Подсказка клавиш (зависит от режима)
        // ============================================================
        if (fontLoaded) {
            int hintSize = (int)std::round(11.0f * S);
            // FIX: было const char* — теперь sf::String
            sf::String hintStr = ui.campaignMode
                ? tr(Loc::HintCampaign, ui.language)
                : tr(Loc::HintSandbox, ui.language);
            sf::Text hint(font, hintStr, hintSize);
            hint.setFillColor(sf::Color(80, 80, 80));
            hint.setPosition({ 12.0f * S, (float)winSize.y - 18.0f * S });
            target.draw(hint);
        }

        if (ui.wallDrawMode) {
            int wmSize = (int)std::round(14.0f * S);
            sf::Text wm(font, tr(Loc::WallMode, ui.language), wmSize);
            sf::FloatRect wb = wm.getLocalBounds();
            wm.setPosition({ (winSize.x - wb.size.x) * 0.5f, PANEL_HEIGHT + 12.0f * S });
            wm.setFillColor(sf::Color(255, 220, 100, 220));
            target.draw(wm);
        }

        // ============================================================
        // Меню спавна
        // ============================================================
        if (ui.spawnMenuOpen) {
            float mLeft = MENU_LEFT * S;
            float mTop = PANEL_HEIGHT + 10.0f * S;
            float mWidth = MENU_WIDTH * S;
            float mHeaderH = MENU_HEADER_H * S;
            float mRowH = MENU_ROW_H * S;
            float mFooterH = MENU_FOOTER_H * S;
            int rowCount;
            if (!ui.campaignMode)                       rowCount = (int)ATOM_TYPES.size();
            else if (ui.campaignLevel == 1)             rowCount = 1;
            else if (ui.campaignLevel == 2)             rowCount = 2;
            else if (ui.campaignLevel == 3)             rowCount = 3;
            else                                        rowCount = (int)ATOM_TYPES.size();
            float mTotalH = mHeaderH + (float)rowCount * mRowH + mFooterH;

            float rIconX = ROW_ICON_X * S;
            float rIconR = ROW_ICON_R * S;
            float rNameX = ROW_NAME_X * S;
            float rMinusX = ROW_MINUS_X * S;
            float rFieldX = ROW_FIELD_X * S;
            float rFieldW = ROW_FIELD_W * S;
            float rPlusX = ROW_PLUS_X * S;
            float rBtnW = ROW_BTN_W * S;
            float rBtnH = ROW_BTN_H * S;

            sf::RectangleShape menuBg(sf::Vector2f(mWidth, mTotalH));
            menuBg.setPosition({ mLeft, mTop });
            menuBg.setFillColor(sf::Color(25, 25, 30, 240));
            menuBg.setOutlineColor(sf::Color(70, 70, 80));
            menuBg.setOutlineThickness(1.0f);
            target.draw(menuBg);

            sf::RectangleShape headerBg(sf::Vector2f(mWidth, mHeaderH));
            headerBg.setPosition({ mLeft, mTop });
            headerBg.setFillColor(sf::Color(40, 40, 50, 255));
            target.draw(headerBg);

            if (fontLoaded) {
                int hSize = (int)std::round(14.0f * S);
                sf::Text headerText(font, tr(Loc::SpawnMenu, ui.language), hSize);
                headerText.setFillColor(sf::Color(220, 220, 220));
                headerText.setPosition({ mLeft + 10.0f * S, mTop + 5.0f * S });
                target.draw(headerText);
            }

            for (size_t i = 0; i < (size_t)rowCount; ++i) {
                float rowTop = mTop + mHeaderH + i * mRowH;
                bool isSelected = ((int)i == ui.selectedSpawnType);
                bool isEditing = ((int)i == ui.editingCountIndex);

                sf::RectangleShape rowBg(sf::Vector2f(mWidth - 4.0f * S, mRowH - 2.0f * S));
                rowBg.setPosition({ mLeft + 2.0f * S, rowTop + 1.0f * S });
                rowBg.setFillColor(isSelected ? sf::Color(70, 130, 210, 120)
                    : sf::Color(35, 35, 40, 120));
                target.draw(rowBg);

                sf::CircleShape icon(rIconR);
                icon.setOrigin({ rIconR, rIconR });
                icon.setPosition({ mLeft + rIconX, rowTop + mRowH / 2.0f });
                icon.setFillColor(ATOM_TYPES[i].color);
                icon.setOutlineColor(atomOutlineColor((int)i));
                icon.setOutlineThickness(1.5f);
                target.draw(icon);

                if (fontLoaded) {
                    int nSize = (int)std::round(14.0f * S);
                    sf::Text nameText(font, locAtomName((int)i, ui.language), nSize);
                    nameText.setFillColor(sf::Color(220, 220, 220));
                    nameText.setPosition({ mLeft + rNameX, rowTop + 7.0f * S });
                    target.draw(nameText);
                }

                float btnY = rowTop + (mRowH - rBtnH) / 2.0f;

                sf::RectangleShape minusBtn(sf::Vector2f(rBtnW, rBtnH));
                minusBtn.setPosition({ mLeft + rMinusX, btnY });
                minusBtn.setFillColor(sf::Color(50, 50, 55));
                minusBtn.setOutlineColor(sf::Color(90, 90, 100));
                minusBtn.setOutlineThickness(1.0f);
                target.draw(minusBtn);

                sf::RectangleShape field(sf::Vector2f(rFieldW, rBtnH));
                field.setPosition({ mLeft + rFieldX, btnY });
                field.setFillColor(isEditing ? sf::Color(30, 50, 75) : sf::Color(25, 25, 30));
                field.setOutlineColor(isEditing ? sf::Color(120, 180, 250) : sf::Color(90, 90, 100));
                field.setOutlineThickness(isEditing ? 2.0f : 1.0f);
                target.draw(field);

                sf::RectangleShape plusBtn(sf::Vector2f(rBtnW, rBtnH));
                plusBtn.setPosition({ mLeft + rPlusX, btnY });
                plusBtn.setFillColor(sf::Color(50, 50, 55));
                plusBtn.setOutlineColor(sf::Color(90, 90, 100));
                plusBtn.setOutlineThickness(1.0f);
                target.draw(plusBtn);

                if (fontLoaded) {
                    int pmSize = (int)std::round(16.0f * S);
                    sf::Text minusText(font, "-", pmSize);
                    sf::FloatRect mbb = minusText.getLocalBounds();
                    minusText.setOrigin({ mbb.position.x + mbb.size.x / 2.0f, mbb.position.y + mbb.size.y / 2.0f });
                    minusText.setPosition({ mLeft + rMinusX + rBtnW / 2.0f, btnY + rBtnH / 2.0f });
                    minusText.setFillColor(sf::Color(200, 200, 200));
                    target.draw(minusText);

                    sf::Text plusText(font, "+", pmSize);
                    sf::FloatRect pbb = plusText.getLocalBounds();
                    plusText.setOrigin({ pbb.position.x + pbb.size.x / 2.0f, pbb.position.y + pbb.size.y / 2.0f });
                    plusText.setPosition({ mLeft + rPlusX + rBtnW / 2.0f, btnY + rBtnH / 2.0f });
                    plusText.setFillColor(sf::Color(200, 200, 200));
                    target.draw(plusText);

                    std::string displayStr;
                    if (isEditing) {
                        displayStr = ui.editBuffer;
                        float t = ui.cursorClock.getElapsedTime().asSeconds();
                        bool cursorOn = (std::fmod(t, 1.0f) < 0.5f);
                        if (cursorOn) displayStr += "|";
                    }
                    else {
                        displayStr = std::to_string(ui.batchCounts[i]);
                    }

                    int cSize = (int)std::round(14.0f * S);
                    sf::Text countText(font, displayStr, cSize);
                    sf::FloatRect cb = countText.getLocalBounds();
                    countText.setOrigin({ cb.position.x + cb.size.x / 2.0f, cb.position.y + cb.size.y / 2.0f });
                    countText.setPosition({ mLeft + rFieldX + rFieldW / 2.0f, rowTop + mRowH / 2.0f });
                    countText.setFillColor(isEditing ? sf::Color(255, 255, 255) : sf::Color(220, 220, 220));
                    target.draw(countText);
                }
            }

            float footerTop = mTop + mHeaderH + (float)rowCount * mRowH;

            if (fontLoaded) {
                int fSize = (int)std::round(12.0f * S);
                sf::Text footerLabel(font, tr(Loc::BatchSpawnBox, ui.language), fSize);
                footerLabel.setFillColor(sf::Color(160, 160, 160));
                footerLabel.setPosition({ mLeft + 10.0f * S, footerTop + 6.0f * S });
                target.draw(footerLabel);
            }

            // В кампании (уровень 1) кнопка Spawn Batch заблокирована,
            // пока не набрано 5 молекул H2 (фаза 1).
            const bool batchDisabled =
                ui.campaignMode && ui.campaignLevel == 1 && ui.taskPhase == 1;

            sf::RectangleShape batchBtn(sf::Vector2f(mWidth - 20.0f * S, 26.0f * S));
            batchBtn.setPosition({ mLeft + 10.0f * S, footerTop + 26.0f * S });
            batchBtn.setFillColor(batchDisabled
                ? sf::Color(70, 70, 75)
                : sf::Color(70, 130, 210));
            batchBtn.setOutlineColor(batchDisabled
                ? sf::Color(110, 110, 115)
                : sf::Color(150, 190, 255));
            batchBtn.setOutlineThickness(1.0f);
            target.draw(batchBtn);

            sf::RectangleShape clearBtn(sf::Vector2f(mWidth - 20.0f * S, 26.0f * S));
            clearBtn.setPosition({ mLeft + 10.0f * S, footerTop + 56.0f * S });
            clearBtn.setFillColor(sf::Color(160, 60, 60));
            clearBtn.setOutlineColor(sf::Color(230, 130, 130));
            clearBtn.setOutlineThickness(1.0f);
            target.draw(clearBtn);

            if (fontLoaded) {
                int bSize = (int)std::round(14.0f * S);
                sf::Text batchText(font, tr(Loc::SpawnBatch, ui.language), bSize);
                sf::FloatRect bb = batchText.getLocalBounds();
                batchText.setOrigin({ bb.position.x + bb.size.x / 2.0f, bb.position.y + bb.size.y / 2.0f });
                batchText.setPosition({ mLeft + mWidth / 2.0f, footerTop + 26.0f * S + 13.0f * S });
                batchText.setFillColor(batchDisabled
                    ? sf::Color(150, 150, 150)
                    : sf::Color::White);
                target.draw(batchText);

                sf::Text clearText(font, tr(Loc::ClearAll, ui.language), bSize);
                sf::FloatRect cb = clearText.getLocalBounds();
                clearText.setOrigin({ cb.position.x + cb.size.x / 2.0f, cb.position.y + cb.size.y / 2.0f });
                clearText.setPosition({ mLeft + mWidth / 2.0f, footerTop + 56.0f * S + 13.0f * S });
                clearText.setFillColor(sf::Color::White);
                target.draw(clearText);

                int mhSize = (int)std::round(11.0f * S);
                sf::Text menuHint(font, tr(Loc::SpawnHint, ui.language), mhSize);
                menuHint.setFillColor(sf::Color(130, 130, 130));
                menuHint.setPosition({ mLeft + 10.0f * S, footerTop + 88.0f * S });
                target.draw(menuHint);
            }
        }

        // ============================================================
        // Панель гравитации (bottom-right)
        // ============================================================
        if (!ui.campaignMode) {
            float gpW = GRAV_PANEL_W * S;
            float gpH = GRAV_PANEL_H * S;
            float gpRight = (float)winSize.x - GRAV_PANEL_RIGHT_MARGIN * S;
            float gpLeft = gpRight - gpW;
            float gpTop = (float)winSize.y - GRAV_PANEL_BOTTOM_OFFSET * S - gpH;

            sf::RectangleShape bg({ gpW, gpH });
            bg.setPosition({ gpLeft, gpTop });
            bg.setFillColor(sf::Color(20, 20, 25, 235));
            bg.setOutlineColor(sf::Color(60, 60, 60));
            bg.setOutlineThickness(1.0f);
            target.draw(bg);

            float row1Y = gpTop + GRAV_ROW1_Y * S;
            float row2Y = gpTop + GRAV_ROW2_Y * S;
            float row3Y = gpTop + GRAV_ROW3_Y * S;
            float slX1 = gpLeft + GRAV_SLIDER_X * S;
            float slX2 = slX1 + GRAV_SLIDER_W * S;

            if (fontLoaded) {
                sf::Text lbl(font, tr(Loc::Gravity, ui.language), (int)std::round(14.0f * S));
                lbl.setFillColor(sf::Color(180, 180, 180));
                sf::FloatRect lb = lbl.getLocalBounds();
                lbl.setOrigin({ lb.position.x, lb.position.y + lb.size.y / 2.0f });
                lbl.setPosition({ gpLeft + GRAV_LABEL_X * S, row1Y });
                target.draw(lbl);
            }

            float cbSize = GRAV_CHECKBOX_SIZE * S;
            float cbX = gpLeft + GRAV_CHECKBOX_X * S;
            float cbY = row1Y - cbSize * 0.5f;
            sf::RectangleShape cbBox({ cbSize, cbSize });
            cbBox.setPosition({ cbX, cbY });
            cbBox.setFillColor(ui.gravityEnabled
                ? sf::Color(70, 130, 210)
                : sf::Color(40, 40, 48));
            cbBox.setOutlineColor(sf::Color(120, 120, 130));
            cbBox.setOutlineThickness(1.5f);
            target.draw(cbBox);
            if (ui.gravityEnabled) {
                sf::VertexArray chk(sf::PrimitiveType::Lines, 4);
                chk[0] = sf::Vertex({ cbX + cbSize * 0.20f, cbY + cbSize * 0.55f }, sf::Color::White);
                chk[1] = sf::Vertex({ cbX + cbSize * 0.45f, cbY + cbSize * 0.80f }, sf::Color::White);
                chk[2] = sf::Vertex({ cbX + cbSize * 0.45f, cbY + cbSize * 0.80f }, sf::Color::White);
                chk[3] = sf::Vertex({ cbX + cbSize * 0.85f, cbY + cbSize * 0.20f }, sf::Color::White);
                target.draw(chk);
            }

            if (fontLoaded) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "dir %.0f°  g %.2f",
                    ui.gravityDirDeg, ui.gravityMagnitude);
                sf::Text info(font, buf, (int)std::round(12.0f * S));
                info.setFillColor(sf::Color(200, 200, 210));
                sf::FloatRect ib = info.getLocalBounds();
                info.setOrigin({ ib.position.x + ib.size.x, ib.position.y + ib.size.y / 2.0f });
                info.setPosition({ gpRight - 12.0f * S, row1Y });
                target.draw(info);
            }

            // Row 2: Angle
            if (fontLoaded) {
                sf::Text lbl(font, tr(Loc::GravityAngle, ui.language), (int)std::round(13.0f * S));
                lbl.setFillColor(sf::Color(180, 180, 180));
                sf::FloatRect lb = lbl.getLocalBounds();
                lbl.setOrigin({ lb.position.x, lb.position.y + lb.size.y / 2.0f });
                lbl.setPosition({ gpLeft + GRAV_LABEL_X * S, row2Y });
                target.draw(lbl);
            }

            sf::RectangleShape trk2({ slX2 - slX1, 4.0f * S });
            trk2.setOrigin({ 0.0f, 2.0f * S });
            trk2.setPosition({ slX1, row2Y });
            trk2.setFillColor(sf::Color(70, 70, 80));
            target.draw(trk2);

            float t2 = ui.gravityDirDeg / 360.0f;
            sf::RectangleShape fill2({ (slX2 - slX1) * t2, 4.0f * S });
            fill2.setOrigin({ 0.0f, 2.0f * S });
            fill2.setPosition({ slX1, row2Y });
            fill2.setFillColor(sf::Color(80, 180, 120));
            target.draw(fill2);

            {
                float cx = slX1 + (slX2 - slX1) * t2;
                float cy = row2Y;
                float rad = ui.gravityDirDeg * 3.14159265f / 180.0f;
                float arrowLen = 10.0f * S;
                float ax = cx + std::cos(rad) * arrowLen;
                float ay = cy + std::sin(rad) * arrowLen;
                sf::VertexArray arrow(sf::PrimitiveType::Lines, 4);
                arrow[0] = sf::Vertex({ cx, cy }, sf::Color(200, 220, 255));
                arrow[1] = sf::Vertex({ ax, ay }, sf::Color(200, 220, 255));
                arrow[2] = sf::Vertex({ ax, ay }, sf::Color(200, 220, 255));
                arrow[3] = sf::Vertex({ cx - std::sin(rad) * 3.0f * S,
                                         cy + std::cos(rad) * 3.0f * S },
                    sf::Color(200, 220, 255));
                target.draw(arrow);
            }

            // Row 3: Magnitude
            if (fontLoaded) {
                sf::Text lbl(font, tr(Loc::GravityForce, ui.language), (int)std::round(13.0f * S));
                lbl.setFillColor(sf::Color(180, 180, 180));
                sf::FloatRect lb = lbl.getLocalBounds();
                lbl.setOrigin({ lb.position.x, lb.position.y + lb.size.y / 2.0f });
                lbl.setPosition({ gpLeft + GRAV_LABEL_X * S, row3Y });
                target.draw(lbl);
            }

            sf::RectangleShape trk3({ slX2 - slX1, 4.0f * S });
            trk3.setOrigin({ 0.0f, 2.0f * S });
            trk3.setPosition({ slX1, row3Y });
            trk3.setFillColor(sf::Color(70, 70, 80));
            target.draw(trk3);

            float t3 = ui.gravityMagnitude / GRAVITY_MAG_MAX;
            sf::RectangleShape fill3({ (slX2 - slX1) * t3, 4.0f * S });
            fill3.setOrigin({ 0.0f, 2.0f * S });
            fill3.setPosition({ slX1, row3Y });
            fill3.setFillColor(sf::Color(80, 180, 120));
            target.draw(fill3);

            float kx = slX1 + (slX2 - slX1) * t3;
            sf::CircleShape knob(GRAV_KNOB_R * S);
            knob.setOrigin({ GRAV_KNOB_R * S, GRAV_KNOB_R * S });
            knob.setPosition({ kx, row3Y });
            knob.setFillColor(sf::Color(120, 220, 160));
            knob.setOutlineColor(sf::Color(220, 255, 230));
            knob.setOutlineThickness(2.0f);
            target.draw(knob);
        }

        // ============================================================
        // Панель размера коробки (bottom-left)
        // ============================================================
        if (!ui.campaignMode) {
            float pLeft = BOX_PANEL_LEFT * S;
            float pWidth = BOX_PANEL_WIDTH * S;
            float pHeight = BOX_PANEL_HEIGHT * S;
            float pTop = (float)winSize.y - BOX_PANEL_BOTTOM_OFFSET * S - pHeight;

            sf::RectangleShape panelBg({ pWidth, pHeight });
            panelBg.setPosition({ pLeft, pTop });
            panelBg.setFillColor(sf::Color(20, 20, 25, 235));
            panelBg.setOutlineColor(sf::Color(60, 60, 60));
            panelBg.setOutlineThickness(1.0f);
            target.draw(panelBg);

            float tX1 = pLeft + BOX_TRACK_X * S;
            float tX2 = tX1 + BOX_TRACK_W * S;
            float row1Y = pTop + BOX_ROW1_Y * S;
            float row2Y = pTop + BOX_ROW2_Y * S;

            // FIX: параметр labelText — sf::String
            auto drawBoxSlider = [&](float size, float rowY, const sf::String& labelText) {
                if (fontLoaded) {
                    sf::Text lbl(font, labelText, (int)std::round(14.0f * S));
                    lbl.setFillColor(sf::Color(180, 180, 180));
                    sf::FloatRect lb = lbl.getLocalBounds();
                    lbl.setOrigin({ lb.position.x, lb.position.y + lb.size.y / 2.0f });
                    lbl.setPosition({ pLeft + BOX_LABEL_X * S, rowY });
                    target.draw(lbl);
                }

                sf::RectangleShape track({ tX2 - tX1, BOX_TRACK_THICKNESS * S });
                track.setOrigin({ 0.0f, BOX_TRACK_THICKNESS * S / 2.0f });
                track.setPosition({ tX1, rowY });
                track.setFillColor(sf::Color(70, 70, 80));
                target.draw(track);

                float t = boxSizeToPos(size);
                sf::RectangleShape fill({ (tX2 - tX1) * t, BOX_TRACK_THICKNESS * S });
                fill.setOrigin({ 0.0f, BOX_TRACK_THICKNESS * S / 2.0f });
                fill.setPosition({ tX1, rowY });
                fill.setFillColor(sf::Color(70, 130, 210));
                target.draw(fill);

                float knobR = BOX_KNOB_R * S;
                sf::CircleShape knob(knobR);
                knob.setOrigin({ knobR, knobR });
                knob.setPosition({ tX1 + (tX2 - tX1) * t, rowY });
                knob.setFillColor(sf::Color(90, 160, 240));
                knob.setOutlineColor(sf::Color(200, 220, 255));
                knob.setOutlineThickness(2.0f);
                target.draw(knob);

                if (fontLoaded) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "%.1f", size);
                    sf::Text val(font, buf, (int)std::round(13.0f * S));
                    val.setFillColor(sf::Color(220, 220, 220));
                    sf::FloatRect vb = val.getLocalBounds();
                    val.setOrigin({ vb.position.x, vb.position.y + vb.size.y / 2.0f });
                    val.setPosition({ pLeft + BOX_VALUE_X * S, rowY });
                    target.draw(val);
                }
                };

            drawBoxSlider(ui.boxSizeX, row1Y, tr(Loc::BoxX, ui.language));
            drawBoxSlider(ui.boxSizeY, row2Y, tr(Loc::BoxY, ui.language));
        }

        // ============================================================
        // Esc / pause menu — поверх всего остального
        // ============================================================
        if (ui.pauseMenuOpen && !ui.pauseSettingsOpen) {
            // Затемняющий оверлей
            sf::RectangleShape overlay({ (float)winSize.x, (float)winSize.y });
            overlay.setFillColor(sf::Color(0, 0, 0, 175));
            target.draw(overlay);

            // Заголовок
            if (fontLoaded) {
                int titleSize = (int)std::round(56.0f * S);
                sf::Text title(font, tr(Loc::Paused, ui.language), titleSize);
                title.setFillColor(sf::Color(225, 235, 255));
                title.setStyle(sf::Text::Bold);
                sf::FloatRect tb = title.getLocalBounds();
                title.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                                  tb.position.y + tb.size.y * 0.5f });
                title.setPosition({ (float)winSize.x * 0.5f,
                                    (float)winSize.y * PAUSE_MENU_TITLE_Y_FRAC });
                target.draw(title);
            }

            // Кнопки
            sf::Vector2i mp = mousePixel;
            sf::Vector2f mpF((float)mp.x, (float)mp.y);
            // FIX: было const char* — tr() теперь sf::String
            sf::String labels[3] = {
                tr(Loc::PauseContinue, ui.language),
                tr(Loc::PauseSettings, ui.language),
                tr(Loc::PauseMainMenu, ui.language)
            };
            for (int i = 0; i < 3; ++i) {
                sf::FloatRect rect = pauseMenuButtonRect(winSize, i);
                bool hovered = rect.contains(mpF);
                drawButton(target, font, fontLoaded, rect, labels[i], hovered, S);
            }
        }

        // Подменю настроек внутри pause-меню
        if (ui.pauseMenuOpen && ui.pauseSettingsOpen) {
            // Затемняем фон
            sf::RectangleShape overlay({ (float)winSize.x, (float)winSize.y });
            overlay.setFillColor(sf::Color(0, 0, 0, 200));
            target.draw(overlay);

            const float sW = 460.0f * S;
            const float sH = 52.0f * S;
            const float sGap = 14.0f * S;
            const float sCX = (float)winSize.x * 0.5f;
            const float sCY = (float)winSize.y * 0.48f;

            // FIX: параметр label — sf::String
            auto drawToggleRow = [&](int idx, const sf::String& label, bool checked) {
                // 3 строки: 0 = Detailed Atoms, 1 = FXAA, 2 = Language
                float y = sCY + (idx - 1.0f) * (sH + sGap);
                sf::FloatRect rect({ sCX - sW * 0.5f, y - sH * 0.5f }, { sW, sH });

                sf::RectangleShape bg({ rect.size.x, rect.size.y });
                bg.setPosition(rect.position);
                bg.setFillColor(sf::Color(30, 35, 45, 230));
                bg.setOutlineColor(sf::Color(80, 90, 110));
                bg.setOutlineThickness(1.5f * S);
                target.draw(bg);

                float cbSz = 22.0f * S;
                float cbX = rect.position.x + 18.0f * S;
                float cbY = rect.position.y + (rect.size.y - cbSz) * 0.5f;
                sf::RectangleShape cb({ cbSz, cbSz });
                cb.setPosition({ cbX, cbY });
                cb.setFillColor(checked ? sf::Color(70, 130, 210) : sf::Color(40, 40, 48));
                cb.setOutlineColor(sf::Color(140, 140, 150));
                cb.setOutlineThickness(1.5f * S);
                target.draw(cb);
                if (checked) {
                    sf::VertexArray chk(sf::PrimitiveType::Lines, 4);
                    chk[0] = sf::Vertex({ cbX + cbSz * 0.20f, cbY + cbSz * 0.55f }, sf::Color::White);
                    chk[1] = sf::Vertex({ cbX + cbSz * 0.45f, cbY + cbSz * 0.80f }, sf::Color::White);
                    chk[2] = sf::Vertex({ cbX + cbSz * 0.45f, cbY + cbSz * 0.80f }, sf::Color::White);
                    chk[3] = sf::Vertex({ cbX + cbSz * 0.85f, cbY + cbSz * 0.20f }, sf::Color::White);
                    target.draw(chk);
                }

                if (fontLoaded) {
                    int fs = (int)std::round(20.0f * S);
                    sf::Text t(font, label, fs);
                    t.setFillColor(sf::Color(230, 235, 245));
                    t.setStyle(sf::Text::Bold);
                    sf::FloatRect tb = t.getLocalBounds();
                    t.setOrigin({ tb.position.x, tb.position.y + tb.size.y * 0.5f });
                    t.setPosition({ cbX + cbSz + 22.0f * S,
                                    rect.position.y + rect.size.y * 0.5f });
                    target.draw(t);
                }
                };

            drawToggleRow(0, tr(Loc::DetailedAtoms, ui.language), ui.showDetailedAtoms);
            drawToggleRow(1, tr(Loc::Fxaa, ui.language), ui.fxaaEnabled);

            // --- Строка «Language»: циклическая кнопка EN → RU → BROKEN ---
            {
                float y = sCY + (2 - 1.0f) * (sH + sGap);
                sf::FloatRect rect({ sCX - sW * 0.5f, y - sH * 0.5f }, { sW, sH });

                sf::RectangleShape bg({ rect.size.x, rect.size.y });
                bg.setPosition(rect.position);
                bg.setFillColor(sf::Color(30, 35, 45, 230));
                bg.setOutlineColor(sf::Color(80, 90, 110));
                bg.setOutlineThickness(1.5f * S);
                target.draw(bg);

                if (fontLoaded) {
                    int fs = (int)std::round(20.0f * S);
                    sf::Text lbl(font, tr(Loc::LanguageLabel, ui.language), fs);
                    lbl.setFillColor(sf::Color(230, 235, 245));
                    lbl.setStyle(sf::Text::Bold);
                    sf::FloatRect lb = lbl.getLocalBounds();
                    lbl.setOrigin({ lb.position.x, lb.position.y + lb.size.y * 0.5f });
                    lbl.setPosition({ rect.position.x + 18.0f * S,
                                      rect.position.y + rect.size.y * 0.5f });
                    target.draw(lbl);

                    // Текущий язык показываем в «правильном» виде,
                    // как в главном меню: RU и BROKEN — через fromUtf8.
                    const LocString* curEntry = &Loc::LangEnglish;
                    Language dispLang = Language::EN;
                    if (ui.language == Language::RU) {
                        curEntry = &Loc::LangRussian;
                        dispLang = Language::RU;
                    }
                    else if (ui.language == Language::BROKEN) {
                        curEntry = &Loc::LangBroken;
                        dispLang = Language::RU;
                    }

                    int vs = (int)std::round(18.0f * S);
                    sf::Text val(font, tr(*curEntry, dispLang), vs);
                    val.setFillColor(sf::Color(220, 230, 245));
                    sf::FloatRect vb = val.getLocalBounds();
                    val.setOrigin({ vb.position.x + vb.size.x,
                                    vb.position.y + vb.size.y * 0.5f });
                    val.setPosition({ rect.position.x + rect.size.x - 28.0f * S,
                                      rect.position.y + rect.size.y * 0.5f });
                    target.draw(val);

                    // Маленькая стрелка справа — намёк на циклический выбор
                    float triH = 6.0f * S;
                    float triW = 10.0f * S;
                    float tx = rect.position.x + rect.size.x - 18.0f * S;
                    float ty = rect.position.y + rect.size.y * 0.5f;
                    sf::ConvexShape tri(3);
                    tri.setPoint(0, { tx - triW * 0.5f, ty - triH * 0.5f });
                    tri.setPoint(1, { tx + triW * 0.5f, ty - triH * 0.5f });
                    tri.setPoint(2, { tx,               ty + triH * 0.5f });
                    tri.setFillColor(sf::Color(180, 195, 220));
                    target.draw(tri);
                }
            }

            // --- Dropdown языка (если открыт) — поверх остальных элементов ---
            if (ui.pauseLanguageDropdownOpen) {
                const float itemH = 40.0f * S;
                float rowY = sCY + (2 - 1.0f) * (sH + sGap);
                float rowBottom = rowY + sH * 0.5f;
                float ix = sCX + sW * 0.5f - sW * 0.55f;
                float iw = sW * 0.55f;

                sf::Vector2f mpF((float)mousePixel.x, (float)mousePixel.y);

                const LocString* items[3] = {
                    &Loc::LangEnglish, &Loc::LangRussian, &Loc::LangBroken
                };
                // Английский — ASCII, русский и «сломанный» — fromUtf8,
                // чтобы «Сломанный» показывался как есть.
                const Language itemDisplayLang[3] = {
                    Language::EN, Language::RU, Language::RU
                };

                for (int i = 0; i < 3; ++i) {
                    float iy = rowBottom + i * itemH;
                    sf::FloatRect ir({ ix, iy }, { iw, itemH });
                    bool hovered = ir.contains(mpF);
                    bool selected = ((int)ui.language == i);

                    sf::RectangleShape ib({ ir.size.x, ir.size.y });
                    ib.setPosition(ir.position);
                    ib.setFillColor(hovered ? sf::Color(70, 130, 210, 240)
                        : (selected ? sf::Color(45, 70, 110, 240)
                            : sf::Color(30, 35, 45, 245)));
                    ib.setOutlineColor(sf::Color(90, 100, 120));
                    ib.setOutlineThickness(1.0f);
                    target.draw(ib);

                    int is = (int)std::round(18.0f * S);
                    sf::Text it(font, tr(*items[i], itemDisplayLang[i]), is);
                    it.setFillColor(sf::Color(230, 235, 245));
                    sf::FloatRect ib2 = it.getLocalBounds();
                    it.setOrigin({ ib2.position.x,
                                   ib2.position.y + ib2.size.y * 0.5f });
                    it.setPosition({ ir.position.x + 14.0f * S,
                                     ir.position.y + ir.size.y * 0.5f });
                    target.draw(it);

                    if (selected) {
                        sf::Text chk(font, "*", is);
                        chk.setFillColor(sf::Color(140, 220, 170));
                        sf::FloatRect cb = chk.getLocalBounds();
                        chk.setOrigin({ cb.position.x + cb.size.x,
                                        cb.position.y + cb.size.y * 0.5f });
                        chk.setPosition({ ir.position.x + ir.size.x - 12.0f * S,
                                          ir.position.y + ir.size.y * 0.5f });
                        target.draw(chk);
                    }
                }
            }

            // Back
            const float bkW = 220.0f * S, bkH = 52.0f * S;
            sf::FloatRect bkRect({ sCX - bkW * 0.5f,
                                    (float)winSize.y * 0.78f - bkH * 0.5f },
                { bkW, bkH });
            sf::RectangleShape bg({ bkRect.size.x, bkRect.size.y });
            bg.setPosition(bkRect.position);
            bg.setFillColor(sf::Color(35, 40, 50, 220));
            bg.setOutlineColor(sf::Color(80, 90, 110));
            bg.setOutlineThickness(2.0f * S);
            target.draw(bg);
            if (fontLoaded) {
                int fs = (int)std::round(24.0f * S);
                sf::Text t(font, tr(Loc::BtnBack, ui.language), fs);
                t.setFillColor(sf::Color::White);
                t.setStyle(sf::Text::Bold);
                sf::FloatRect tb = t.getLocalBounds();
                t.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                              tb.position.y + tb.size.y * 0.5f });
                t.setPosition({ bkRect.position.x + bkRect.size.x * 0.5f,
                                bkRect.position.y + bkRect.size.y * 0.5f });
                target.draw(t);
            }

            // Заголовок и подсказка
            if (fontLoaded) {
                int ts = (int)std::round(44.0f * S);
                sf::Text title(font, tr(Loc::BtnSettings, ui.language), ts);
                title.setFillColor(sf::Color(225, 235, 255));
                title.setStyle(sf::Text::Bold);
                sf::FloatRect tb = title.getLocalBounds();
                title.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                                  tb.position.y + tb.size.y * 0.5f });
                title.setPosition({ sCX, (float)winSize.y * 0.28f });
                target.draw(title);

                int hs = (int)std::round(13.0f * S);
                sf::Text hint(font, tr(Loc::EscBack, ui.language), hs);
                hint.setFillColor(sf::Color(150, 165, 190));
                sf::FloatRect hb = hint.getLocalBounds();
                hint.setOrigin({ hb.position.x + hb.size.x * 0.5f,
                                 hb.position.y + hb.size.y * 0.5f });
                hint.setPosition({ sCX, (float)winSize.y - 32.0f * S });
                target.draw(hint);
            }
        }
    } // конец блока DrawPass::UI / All
}
