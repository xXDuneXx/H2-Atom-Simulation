#include "MainMenu.hpp"
#include "Config.hpp"
#include "Physics.hpp"
#include "Localization.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#endif

namespace {

    // ============================================================
    // Параметры демо-сцены
    // ============================================================
    const float DEMO_BOX_SIZE = 20.0f;
    const float DEMO_RESTART_TIME = 45.0f;
    const int   DEMO_SUBSTEPS = 20;
    const float DEMO_TEMP_C = -50.0f;

    const int   H_ONLY_COUNT = 60;
    const int   H_MIX_COUNT = 40;
    const int   O_MIX_COUNT = 20;

    // ============================================================
    // Layout
    // ============================================================
    const float DEMO_VP_LEFT = 0.00f;
    const float DEMO_VP_TOP = 0.00f;
    const float DEMO_VP_WIDTH = 0.48f;
    const float DEMO_VP_HEIGHT = 1.00f;

    const float OVERLAY_LEFT_FRAC = 0.48f;

    const float BTN_W_SCALE = 360.0f;
    const float BTN_H_SCALE = 64.0f;
    const float BTN_GAP_SCALE = 18.0f;
    const float BTN_CX_FRAC = 0.73f;
    const float BTN_START_Y_FRAC = 0.52f;

    const float SKIP_BTN_W_SCALE = 56.0f;
    const float SKIP_BTN_H_SCALE = 44.0f;
    const float SKIP_BTN_PAD_SCALE = 10.0f;

    const float YT_ICON_SIZE_SCALE = 50.0f;
    const float YT_ICON_MARGIN_SCALE = 20.0f;
    const char* YT_URL = "https://www.youtube.com/@zlobniy-b4u";

    // ============================================================
    // ModeSelect layout
    // ============================================================
    const float MODE_BTN_W_SCALE = 260.0f;
    const float MODE_BTN_H_SCALE = 220.0f;
    const float MODE_BTN_GAP_SCALE = 100.0f;
    const float MODE_BTN_CY_FRAC = 0.52f;

    const float MODE_ICON_MUL = 2.4f;
    const float MODE_ICON_YFRAC = 0.40f;
    const float MODE_TEXT_YFRAC = 1.30f;
    const float MODE_HEADING_MARGIN_SCALE = 28.0f;

    const float MODE_BACK_W_SCALE = 220.0f;
    const float MODE_BACK_H_SCALE = 52.0f;
    const float MODE_BACK_BOTTOM_MARGIN_SCALE = 40.0f;

    // ============================================================
    // LevelSelect layout
    // ============================================================
    const float LEVEL_BTN_W_SCALE = 560.0f;
    const float LEVEL_BTN_H_SCALE = 76.0f;
    const float LEVEL_BTN_GAP_SCALE = 16.0f;
    const float LEVEL_BTN_CY_FRAC = 0.52f;
    const float LEVEL_HEADING_Y_FRAC = 0.30f;

    // Тинт температуры
    const float TINT_NEUTRAL_C = 15.0f;
    const float TINT_SATURATE_DELTA_C = 42.0f;
    const float TINT_MAX_ALPHA = 0.13f;

    const float TITLE_DIM_MODESELECT = 0.30f;

    // --- Settings layout ---
    const float SETTINGS_ROW_W_SCALE = 460.0f;
    const float SETTINGS_ROW_H_SCALE = 52.0f;
    const float SETTINGS_ROW_GAP_SCALE = 14.0f;
    const float SETTINGS_CHECKBOX_SCALE = 22.0f;
    const float SETTINGS_CY_FRAC = 0.48f;
    const float SETTINGS_HEADING_Y_FRAC = 0.28f;
    const float SETTINGS_BACK_W_SCALE = 200.0f;
    const float SETTINGS_BACK_H_SCALE = 52.0f;

    sf::Color lerpColor(const sf::Color& a, const sf::Color& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return sf::Color(
            (std::uint8_t)(a.r + (b.r - a.r) * t),
            (std::uint8_t)(a.g + (b.g - a.g) * t),
            (std::uint8_t)(a.b + (b.b - a.b) * t));
    }

    void drawVignette(sf::RenderWindow& window, sf::Vector2u winSize,
        sf::Color color, float intensity) {
        if (intensity <= 0.001f) return;
        sf::Vector2f center(winSize.x * 0.5f, winSize.y * 0.5f);
        float maxR = std::sqrt(center.x * center.x + center.y * center.y);
        const int SEGMENTS = 64;
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan, SEGMENTS + 2);
        sf::Color centerColor = color; centerColor.a = 0;
        fan[0] = sf::Vertex(center, centerColor);
        sf::Color edgeColor = color;
        edgeColor.a = (std::uint8_t)std::clamp(intensity * 255.0f, 0.0f, 255.0f);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (float)i / SEGMENTS * 2.0f * 3.14159265f;
            float x = center.x + std::cos(angle) * maxR;
            float y = center.y + std::sin(angle) * maxR;
            fan[i + 1] = sf::Vertex({ x, y }, edgeColor);
        }
        window.draw(fan);
    }

    void openUrl(const char* url) {
#ifdef _WIN32
        ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#else
        (void)url;
#endif
    }

    void computeButtonRects(sf::Vector2u winSize,
        sf::FloatRect& a, sf::FloatRect& b, sf::FloatRect& c)
    {
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float btnW = BTN_W_SCALE * S;
        float btnH = BTN_H_SCALE * S;
        float gap = BTN_GAP_SCALE * S;
        float cx = (float)winSize.x * BTN_CX_FRAC;
        float startY = (float)winSize.y * BTN_START_Y_FRAC;
        a = sf::FloatRect({ cx - btnW * 0.5f, startY }, { btnW, btnH });
        b = sf::FloatRect({ cx - btnW * 0.5f, startY + btnH + gap }, { btnW, btnH });
        c = sf::FloatRect({ cx - btnW * 0.5f, startY + 2.0f * (btnH + gap) }, { btnW, btnH });
    }

    sf::FloatRect computeSkipRect(sf::Vector2u winSize) {
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float btnW = SKIP_BTN_W_SCALE * S;
        float btnH = SKIP_BTN_H_SCALE * S;
        float pad = SKIP_BTN_PAD_SCALE * S;
        float vpRight = (DEMO_VP_LEFT + DEMO_VP_WIDTH) * (float)winSize.x;
        float vpTop = DEMO_VP_TOP * (float)winSize.y;
        return sf::FloatRect({ vpRight - btnW - pad, vpTop + pad }, { btnW, btnH });
    }

    sf::FloatRect computeYtRect(sf::Vector2u winSize) {
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float sz = YT_ICON_SIZE_SCALE * S;
        float m = YT_ICON_MARGIN_SCALE * S;
        return sf::FloatRect({ (float)winSize.x - sz - m, (float)winSize.y - sz - m },
            { sz, sz });
    }

    sf::FloatRect computeModeButtonRect(sf::Vector2u winSize, int idx) {
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float btnW = MODE_BTN_W_SCALE * S;
        float btnH = MODE_BTN_H_SCALE * S;
        float gap = MODE_BTN_GAP_SCALE * S;
        float cx = (float)winSize.x * 0.5f;
        float cy = (float)winSize.y * MODE_BTN_CY_FRAC;
        float totalW = 2.0f * btnW + gap;
        float leftX = cx - totalW * 0.5f;
        float x = leftX + idx * (btnW + gap);
        return sf::FloatRect({ x, cy - btnH * 0.5f }, { btnW, btnH });
    }

    sf::FloatRect computeModeBackRect(sf::Vector2u winSize) {
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float w = MODE_BACK_W_SCALE * S;
        float h = MODE_BACK_H_SCALE * S;
        float cx = (float)winSize.x * 0.5f;
        float cy = (float)winSize.y - MODE_BACK_BOTTOM_MARGIN_SCALE * S - h * 0.5f;
        return sf::FloatRect({ cx - w * 0.5f, cy - h * 0.5f }, { w, h });
    }

    const int LEVEL_COUNT = 5;

    sf::FloatRect computeLevelButtonRect(sf::Vector2u winSize, int idx) {
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float w = LEVEL_BTN_W_SCALE * S;
        float h = LEVEL_BTN_H_SCALE * S;
        float gap = LEVEL_BTN_GAP_SCALE * S;
        float cx = (float)winSize.x * 0.5f;
        float cy = (float)winSize.y * LEVEL_BTN_CY_FRAC;

        float totalH = LEVEL_COUNT * h + (LEVEL_COUNT - 1) * gap;
        float startY = cy - totalH * 0.5f;
        float y = startY + idx * (h + gap);
        return sf::FloatRect({ cx - w * 0.5f, y }, { w, h });
    }

    // --- заголовок кнопки уровня: принимает sf::String ---
    void drawLevelButton(sf::RenderWindow& window, const sf::Font& font,
        bool fontLoaded, const sf::FloatRect& rect,
        int number, const sf::String& title, const sf::String& subtitle,
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
        window.draw(bg);

        if (!fontLoaded) return;

        int numSize = (int)std::round(34.0f * S);
        sf::Text num(font, std::to_string(number), numSize);
        num.setFillColor(hovered ? sf::Color::White : sf::Color(180, 195, 220));
        num.setStyle(sf::Text::Bold);
        sf::FloatRect nb = num.getLocalBounds();
        num.setOrigin({ nb.position.x + nb.size.x * 0.5f,
                        nb.position.y + nb.size.y * 0.5f });
        num.setPosition({ rect.position.x + 44.0f * S,
                          rect.position.y + rect.size.y * 0.5f });
        window.draw(num);

        int titleSize = (int)std::round(22.0f * S);
        sf::Text t(font, title, titleSize);
        t.setFillColor(sf::Color::White);
        t.setStyle(sf::Text::Bold);
        t.setPosition({ rect.position.x + 84.0f * S,
                        rect.position.y + 12.0f * S });
        window.draw(t);

        if (!subtitle.isEmpty()) {                        // sf::String → isEmpty()
            int subSize = (int)std::round(13.0f * S);
            sf::Text st(font, subtitle, subSize);
            st.setFillColor(sf::Color(150, 165, 190));
            st.setPosition({ rect.position.x + 84.0f * S,
                             rect.position.y + 44.0f * S });
            window.draw(st);
        }
    }

    sf::FloatRect computeSettingsRowRect(sf::Vector2u winSize, int idx) {
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float w = SETTINGS_ROW_W_SCALE * S;
        float h = SETTINGS_ROW_H_SCALE * S;
        float gap = SETTINGS_ROW_GAP_SCALE * S;
        float cx = (float)winSize.x * 0.5f;
        float cy = (float)winSize.y * SETTINGS_CY_FRAC;
        float y = cy + (idx - 1.0f) * (h + gap);
        return sf::FloatRect({ cx - w * 0.5f, y - h * 0.5f }, { w, h });
    }

    // Прямоугольник пункта выпадающего списка языка.
    // idx = 0 → English, idx = 1 → Русский, idx = 2 → Сломанный.
    sf::FloatRect computeLanguageItemRect(sf::Vector2u winSize, int idx) {
        sf::FloatRect row = computeSettingsRowRect(winSize, 2);
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float itemH = 40.0f * S;
        float x = row.position.x + row.size.x * 0.45f;
        float w = row.size.x * 0.55f;
        float y = row.position.y + row.size.y + idx * itemH;
        return sf::FloatRect({ x, y }, { w, itemH });
    }

    sf::FloatRect computeSettingsBackRect(sf::Vector2u winSize) {
        float S = std::max(1.0f, (float)winSize.y / 1080.0f);
        float w = SETTINGS_BACK_W_SCALE * S;
        float h = SETTINGS_BACK_H_SCALE * S;
        float cx = (float)winSize.x * 0.5f;
        float cy = (float)winSize.y * 0.78f;
        return sf::FloatRect({ cx - w * 0.5f, cy - h * 0.5f }, { w, h });
    }

    // --- строка toggle в Settings: принимает sf::String ---
    void drawSettingsRow(sf::RenderWindow& window, const sf::Font& font,
        bool fontLoaded, const sf::FloatRect& rect,
        const sf::String& label, bool checked, bool hovered, float S)
    {
        sf::RectangleShape bg({ rect.size.x, rect.size.y });
        bg.setPosition(rect.position);
        bg.setFillColor(hovered ? sf::Color(45, 60, 90, 230)
            : sf::Color(30, 35, 45, 220));
        bg.setOutlineColor(hovered ? sf::Color(120, 170, 230)
            : sf::Color(70, 80, 100));
        bg.setOutlineThickness(1.5f * S);
        window.draw(bg);

        float cbSz = SETTINGS_CHECKBOX_SCALE * S;
        float cbX = rect.position.x + 18.0f * S;
        float cbY = rect.position.y + (rect.size.y - cbSz) * 0.5f;

        sf::RectangleShape cb({ cbSz, cbSz });
        cb.setPosition({ cbX, cbY });
        cb.setFillColor(checked ? sf::Color(70, 130, 210)
            : sf::Color(40, 40, 48));
        cb.setOutlineColor(sf::Color(140, 140, 150));
        cb.setOutlineThickness(1.5f * S);
        window.draw(cb);

        if (checked) {
            sf::VertexArray chk(sf::PrimitiveType::Lines, 4);
            chk[0] = sf::Vertex({ cbX + cbSz * 0.20f, cbY + cbSz * 0.55f }, sf::Color::White);
            chk[1] = sf::Vertex({ cbX + cbSz * 0.45f, cbY + cbSz * 0.80f }, sf::Color::White);
            chk[2] = sf::Vertex({ cbX + cbSz * 0.45f, cbY + cbSz * 0.80f }, sf::Color::White);
            chk[3] = sf::Vertex({ cbX + cbSz * 0.85f, cbY + cbSz * 0.20f }, sf::Color::White);
            window.draw(chk);
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
            window.draw(t);
        }
    }

    // ============================================================
    // Строка "Language" с выпадающим списком из ТРЁХ пунктов.
    //
    // Каждый пункт отображается «в своём правильном виде»:
    //   English  → чистая латиница (ASCII)
    //   Русский  → декодируется fromUtf8 → «Русский»
    //   Сломанный→ байты LangBroken.en/ru как есть → mojibake
    //
    // Заголовок строки «Language / Язык / <mojibake>» зависит
    // от текущего языка: tr(LanguageLabel, current).
    // ============================================================
    void drawLanguageRow(sf::RenderWindow& window, const sf::Font& font,
        bool fontLoaded, const sf::FloatRect& rect,
        Language current, bool open, int hoveredItem,
        bool hoveredRow, float S)
    {
        sf::RectangleShape bg({ rect.size.x, rect.size.y });
        bg.setPosition(rect.position);
        bg.setFillColor(hoveredRow ? sf::Color(45, 60, 90, 230)
            : sf::Color(30, 35, 45, 220));
        bg.setOutlineColor(hoveredRow ? sf::Color(120, 170, 230)
            : sf::Color(70, 80, 100));
        bg.setOutlineThickness(1.5f * S);
        window.draw(bg);

        if (!fontLoaded) return;

        // --- Заголовок строки: зависит от языка ---
        // EN → "Language"; RU → "Язык"; BROKEN → mojibake-байты как ANSI.
        int fs = (int)std::round(20.0f * S);
        sf::Text lbl(font, tr(Loc::LanguageLabel, current), fs);
        lbl.setFillColor(sf::Color(230, 235, 245));
        lbl.setStyle(sf::Text::Bold);
        sf::FloatRect lb = lbl.getLocalBounds();
        lbl.setOrigin({ lb.position.x, lb.position.y + lb.size.y * 0.5f });
        lbl.setPosition({ rect.position.x + 18.0f * S,
                          rect.position.y + rect.size.y * 0.5f });
        window.draw(lbl);

        // --- Текущее выбранное значение ---
        // Для каждого языка подбираем «правильную» пару (entry, lang),
        // чтобы метка отрисовалась читаемо:
        //   EN      → ASCII
        //   RU      → fromUtf8 ("Русский")
        //   BROKEN  → fromUtf8 (метка — та же mojibake-строка)
        const LocString* curEntry = nullptr;
        Language curDisplayLang = Language::EN;
        if (current == Language::EN) {
            curEntry = &Loc::LangEnglish;
            curDisplayLang = Language::EN;
        }
        else if (current == Language::RU) {
            curEntry = &Loc::LangRussian;
            curDisplayLang = Language::RU;
        }
        else {
            curEntry = &Loc::LangBroken;
            curDisplayLang = Language::RU;   // fromUtf8 — чтобы mojibake-текст
            // корректно лёг на экран
        }

        int vs = (int)std::round(18.0f * S);
        sf::Text val(font, tr(*curEntry, curDisplayLang), vs);
        val.setFillColor(sf::Color(220, 230, 245));
        sf::FloatRect vb = val.getLocalBounds();
        val.setOrigin({ vb.position.x + vb.size.x, vb.position.y + vb.size.y * 0.5f });
        val.setPosition({ rect.position.x + rect.size.x - 28.0f * S,
                          rect.position.y + rect.size.y * 0.5f });
        window.draw(val);

        // --- Стрелка ---
        float triH = 6.0f * S;
        float triW = 10.0f * S;
        float tx = rect.position.x + rect.size.x - 18.0f * S;
        float ty = rect.position.y + rect.size.y * 0.5f;
        sf::ConvexShape tri(3);
        tri.setPoint(0, { tx - triW * 0.5f, ty - triH * 0.5f });
        tri.setPoint(1, { tx + triW * 0.5f, ty - triH * 0.5f });
        tri.setPoint(2, { tx,               ty + triH * 0.5f });
        tri.setFillColor(sf::Color(180, 195, 220));
        window.draw(tri);

        // --- Открытый дропдаун: три пункта ---
        if (open) {
            const LocString* items[3] = {
                &Loc::LangEnglish, &Loc::LangRussian, &Loc::LangBroken
            };
            // Для пункта отображения: первые два — ASCII / fromUtf8,
            // третий — тоже fromUtf8 (это «правильный» способ показать
            // уже-испорченные символы).
            const Language itemDisplayLang[3] = {
                Language::EN, Language::RU, Language::RU
            };

            for (int i = 0; i < 3; ++i) {
                float itemH = 40.0f * S;
                float ix = rect.position.x + rect.size.x * 0.45f;
                float iw = rect.size.x * 0.55f;
                float iy = rect.position.y + rect.size.y + i * itemH;
                sf::FloatRect ir({ ix, iy }, { iw, itemH });

                sf::RectangleShape ib({ ir.size.x, ir.size.y });
                ib.setPosition(ir.position);
                bool sel = ((int)current == i);
                bool hov = (hoveredItem == i);
                ib.setFillColor(hov ? sf::Color(70, 130, 210, 240)
                    : (sel ? sf::Color(45, 70, 110, 240)
                        : sf::Color(30, 35, 45, 245)));
                ib.setOutlineColor(sf::Color(90, 100, 120));
                ib.setOutlineThickness(1.0f);
                window.draw(ib);

                int is = (int)std::round(18.0f * S);
                sf::Text it(font, tr(*items[i], itemDisplayLang[i]), is);
                it.setFillColor(sf::Color(230, 235, 245));
                sf::FloatRect ib2 = it.getLocalBounds();
                it.setOrigin({ ib2.position.x, ib2.position.y + ib2.size.y * 0.5f });
                it.setPosition({ ir.position.x + 14.0f * S,
                                 ir.position.y + ir.size.y * 0.5f });
                window.draw(it);

                if (sel) {
                    sf::Text chk(font, "*", is);
                    chk.setFillColor(sf::Color(140, 220, 170));
                    sf::FloatRect cb = chk.getLocalBounds();
                    chk.setOrigin({ cb.position.x + cb.size.x,
                                    cb.position.y + cb.size.y * 0.5f });
                    chk.setPosition({ ir.position.x + ir.size.x - 12.0f * S,
                                      ir.position.y + ir.size.y * 0.5f });
                    window.draw(chk);
                }
            }
        }
    }

    float computeModeIconTopY(sf::Vector2u winSize) {
        sf::FloatRect r = computeModeButtonRect(winSize, 0);
        float halfH = r.size.y * 0.275f * MODE_ICON_MUL;
        return r.position.y + r.size.y * MODE_ICON_YFRAC - halfH;
    }

    // ============================================================
    // Иконки
    // ============================================================
    void drawSandboxIcon(sf::RenderWindow& window, sf::Vector2f c,
        float size, sf::Color color, float S)
    {
        float w = size * 0.72f;
        float h = size * 0.62f;
        float thick = std::max(2.0f * S, size * 0.055f);
        sf::RectangleShape box({ w, h });
        box.setOrigin({ w * 0.5f, h * 0.5f });
        box.setPosition(c);
        box.setFillColor(sf::Color::Transparent);
        box.setOutlineColor(color);
        box.setOutlineThickness(thick);
        window.draw(box);

        const float px[] = { -0.22f,  0.04f,  0.24f, -0.06f };
        const float py[] = { -0.14f, -0.20f, -0.02f,  0.15f };
        const float pr[] = { 0.10f,  0.075f, 0.085f, 0.11f };
        for (int i = 0; i < 4; ++i) {
            float r = size * pr[i];
            sf::CircleShape circle(r);
            circle.setOrigin({ r, r });
            circle.setPosition({ c.x + size * px[i], c.y + size * py[i] });
            circle.setFillColor(color);
            window.draw(circle);
        }
    }

    // ============================================================
    // Кнопки — принимают sf::String
    // ============================================================
    void drawButton(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
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
        window.draw(bg);
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
            window.draw(text);
        }
    }

    void drawModeButton(sf::RenderWindow& window, const sf::Font& font,
        bool fontLoaded, const sf::FloatRect& rect,
        const sf::String& label, bool hovered,
        float S, int modeIdx,
        const sf::Texture* campaignTex, bool campaignTexLoaded,
        const sf::Texture* sandboxTex, bool sandboxTexLoaded)
    {
        sf::Vector2f iconCenter = {
            rect.position.x + rect.size.x * 0.5f,
            rect.position.y + rect.size.y * MODE_ICON_YFRAC
        };
        float iconArea = rect.size.y * 0.55f * MODE_ICON_MUL;

        const sf::Texture* tex = (modeIdx == 0) ? campaignTex : sandboxTex;
        bool texOk = (modeIdx == 0) ? campaignTexLoaded : sandboxTexLoaded;

        if (texOk && tex && tex->getSize().x > 0 && tex->getSize().y > 0) {
            float texW = (float)tex->getSize().x;
            float texH = (float)tex->getSize().y;
            float scale = std::min(iconArea / texW, iconArea / texH);

            sf::Sprite spr(*tex);
            spr.setScale({ scale, scale });
            spr.setOrigin({ texW * 0.5f, texH * 0.5f });
            spr.setPosition(iconCenter);
            window.draw(spr);

            if (hovered) {
                float bw = texW * scale + 8.0f * S;
                float bh = texH * scale + 8.0f * S;
                sf::RectangleShape border({ bw, bh });
                border.setOrigin({ bw * 0.5f, bh * 0.5f });
                border.setPosition(iconCenter);
                border.setFillColor(sf::Color::Transparent);
                border.setOutlineColor(sf::Color(255, 255, 255, 200));
                border.setOutlineThickness(2.0f * S);
                window.draw(border);
            }
        }
        else if (modeIdx == 1) {
            sf::Color iconColor = hovered ? sf::Color::White
                : sf::Color(200, 220, 245);
            drawSandboxIcon(window, iconCenter, iconArea * 0.8f,
                iconColor, S);
        }

        if (fontLoaded) {
            int fontSize = (int)std::round(22.0f * S);
            sf::Text text(font, label, fontSize);
            text.setFillColor(sf::Color::White);
            text.setStyle(sf::Text::Bold);
            sf::FloatRect tb = text.getLocalBounds();
            text.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                             tb.position.y + tb.size.y * 0.5f });
            text.setPosition({ rect.position.x + rect.size.x * 0.5f,
                               rect.position.y + rect.size.y * MODE_TEXT_YFRAC });
            window.draw(text);
        }
    }

    void drawSkipButton(sf::RenderWindow& window,
        const sf::FloatRect& rect,
        bool hovered, float S)
    {
        sf::RectangleShape bg({ rect.size.x, rect.size.y });
        bg.setPosition(rect.position);
        if (hovered) {
            bg.setFillColor(sf::Color(70, 120, 200, 235));
            bg.setOutlineColor(sf::Color(150, 200, 255, 255));
        }
        else {
            bg.setFillColor(sf::Color(25, 30, 40, 220));
            bg.setOutlineColor(sf::Color(80, 90, 110, 255));
        }
        bg.setOutlineThickness(2.0f * S);
        window.draw(bg);

        float cx = rect.position.x + rect.size.x * 0.5f;
        float cy = rect.position.y + rect.size.y * 0.5f;
        float triH = rect.size.y * 0.40f;
        float triW = rect.size.x * 0.22f;
        float totalW = 2.0f * triW;
        float x0 = cx - totalW * 0.5f;

        sf::Color triCol = hovered ? sf::Color::White
            : sf::Color(220, 230, 245);
        for (int k = 0; k < 2; ++k) {
            float xb = x0 + k * triW;
            sf::ConvexShape tri(3);
            tri.setPoint(0, { xb,          cy - triH * 0.5f });
            tri.setPoint(1, { xb + triW,   cy });
            tri.setPoint(2, { xb,          cy + triH * 0.5f });
            tri.setFillColor(triCol);
            window.draw(tri);
        }
    }

    void drawYtIcon(sf::RenderWindow& window,
        const sf::FloatRect& rect,
        bool hovered,
        const sf::Texture* tex, bool texLoaded,
        float S)
    {
        if (texLoaded && tex && tex->getSize().x > 0 && tex->getSize().y > 0) {
            sf::Sprite spr(*tex);
            spr.setPosition(rect.position);
            sf::Vector2u ts = tex->getSize();
            spr.setScale({ rect.size.x / (float)ts.x,
                           rect.size.y / (float)ts.y });
            window.draw(spr);

            if (hovered) {
                sf::RectangleShape border({ rect.size.x, rect.size.y });
                border.setPosition(rect.position);
                border.setFillColor(sf::Color::Transparent);
                border.setOutlineColor(sf::Color(255, 255, 255, 230));
                border.setOutlineThickness(3.0f * S);
                window.draw(border);
            }
        }
        else {
            sf::RectangleShape bg({ rect.size.x, rect.size.y });
            bg.setPosition(rect.position);
            bg.setFillColor(hovered ? sf::Color(220, 30, 30)
                : sf::Color(180, 30, 30));
            bg.setOutlineColor(hovered ? sf::Color::White
                : sf::Color(80, 30, 30));
            bg.setOutlineThickness(2.0f * S);
            window.draw(bg);

            float cx = rect.position.x + rect.size.x * 0.5f;
            float cy = rect.position.y + rect.size.y * 0.5f;
            float triH = rect.size.y * 0.55f;
            float triW = rect.size.x * 0.40f;
            sf::ConvexShape tri(3);
            tri.setPoint(0, { cx - triW * 0.35f, cy - triH * 0.5f });
            tri.setPoint(1, { cx + triW * 0.65f, cy });
            tri.setPoint(2, { cx - triW * 0.35f, cy + triH * 0.5f });
            tri.setFillColor(sf::Color::White);
            window.draw(tri);
        }
    }

} // namespace

// ============================================================
// Инициализация
// ============================================================

MainMenu::MainMenu() {}

void MainMenu::init(std::mt19937& rng) {
    m_grid.init(-DEMO_BOX_SIZE * 0.5f, DEMO_BOX_SIZE, VDW_CUTOFF);
    restartDemo(rng);
    m_restartTimer = DEMO_RESTART_TIME;

    m_view = View::Main;
    m_languageDropdownOpen = false;
    m_languageDropdownHovered = -1;

    if (!m_ytLoaded) {
        m_ytLoaded = m_ytTexture.loadFromFile("youtubee.png");
    }
    if (!m_campaignLoaded) {
        m_campaignLoaded = m_campaignTexture.loadFromFile("Campaign.jpg");
    }
    if (!m_sandboxLoaded) {
        m_sandboxLoaded = m_sandboxTexture.loadFromFile("Sandbox.jpg");
    }
}

void MainMenu::restartDemo(std::mt19937& rng) {
    m_atoms.clear();
    m_atoms.reserve(H_ONLY_COUNT);

    std::uniform_real_distribution<float> posDist(-8.5f, 8.5f);
    std::uniform_real_distribution<float> velDist(-0.4f, 0.4f);
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);

    bool oxygenMix = (coin(rng) < 0.5f);

    auto spawnOne = [&](int elementId) {
        float x = 0.0f, y = 0.0f;
        for (int attempt = 0; attempt < 100; ++attempt) {
            x = posDist(rng);
            y = posDist(rng);
            bool free = true;
            for (const auto& a : m_atoms) {
                float dx = a.pos.x - x;
                float dy = a.pos.y - y;
                if (dx * dx + dy * dy < 1.0f) { free = false; break; }
            }
            if (free) break;
        }
        m_atoms.push_back(makeAtom(elementId, { x, y },
            { velDist(rng), velDist(rng) }));
        };

    if (oxygenMix) {
        for (int i = 0; i < H_MIX_COUNT; ++i) spawnOne(0);
        for (int i = 0; i < O_MIX_COUNT; ++i) spawnOne(1);
    }
    else {
        for (int i = 0; i < H_ONLY_COUNT; ++i) spawnOne(0);
    }
}

// ============================================================
// Физика демо
// ============================================================

void MainMenu::stepDemoPhysics(std::mt19937& rng) {
    m_grid.clear();
    for (size_t k = 0; k < m_atoms.size(); ++k)
        m_grid.insert((int)k, m_atoms[k].pos);

    updateBonds(m_atoms, m_grid);

    const float physTemp = tempCelsiusToPhysics(DEMO_TEMP_C);
    updateHBonds(m_atoms, m_grid, physTemp, rng);

    std::vector<sf::Vector2f> forces(m_atoms.size(), { 0.0f, 0.0f });

    for (size_t i = 0; i < m_atoms.size(); ++i) {
        int cx = m_grid.cellX(m_atoms[i].pos.x);
        int cy = m_grid.cellY(m_atoms[i].pos.y);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx, ny = cy + dy;
                if (nx < 0 || nx >= m_grid.cols() ||
                    ny < 0 || ny >= m_grid.rows()) continue;
                for (int j : m_grid.at(nx, ny)) {
                    if (j <= (int)i) continue;
                    float fx, fy;
                    computeInteraction(m_atoms, (int)i, j, fx, fy);
                    forces[i].x -= fx; forces[i].y -= fy;
                    forces[j].x += fx; forces[j].y += fy;
                }
            }
        }
    }

    applyHBondAngularForces(m_atoms, forces);
    applyWaterAngleForces(m_atoms, forces);
    applyPeroxideAngleForces(m_atoms, forces);

    static std::normal_distribution<float> normalDist(0.0f, 1.0f);
    const float half = DEMO_BOX_SIZE * 0.5f;

    for (size_t i = 0; i < m_atoms.size(); ++i) {
        sf::Vector2f accel = forces[i] / m_atoms[i].mass;
        m_atoms[i].vel += accel * PHYS_DT;

        float noiseAmp = std::sqrt(2.0f * GAMMA * physTemp / m_atoms[i].mass * PHYS_DT);
        float vxNoise = noiseAmp * normalDist(rng);
        float vyNoise = noiseAmp * normalDist(rng);

        m_atoms[i].vel.x += -GAMMA * m_atoms[i].vel.x * PHYS_DT + vxNoise;
        m_atoms[i].vel.y += -GAMMA * m_atoms[i].vel.y * PHYS_DT + vyNoise;

        m_atoms[i].vel *= GLOBAL_DAMPING;
        m_atoms[i].pos += m_atoms[i].vel * PHYS_DT;
        m_atoms[i].age += PHYS_DT * 0.1f;
    }

    for (auto& a : m_atoms) {
        if (a.pos.x < -half + a.radius) { a.pos.x = -half + a.radius; a.vel.x = -a.vel.x * BOUNDARY_REST; }
        if (a.pos.x > half - a.radius) { a.pos.x = half - a.radius; a.vel.x = -a.vel.x * BOUNDARY_REST; }
        if (a.pos.y < -half + a.radius) { a.pos.y = -half + a.radius; a.vel.y = -a.vel.y * BOUNDARY_REST; }
        if (a.pos.y > half - a.radius) { a.pos.y = half - a.radius; a.vel.y = -a.vel.y * BOUNDARY_REST; }
    }
}

void MainMenu::update(float dt, std::mt19937& rng) {
    m_restartTimer -= dt;
    if (m_restartTimer <= 0.0f) {
        restartDemo(rng);
        m_restartTimer = DEMO_RESTART_TIME;
    }
    for (int i = 0; i < DEMO_SUBSTEPS; ++i)
        stepDemoPhysics(rng);
}

// ============================================================
// Ввод
// ============================================================

void MainMenu::updateHover(sf::Vector2i mousePos, sf::Vector2u winSize) {
    sf::Vector2f m((float)mousePos.x, (float)mousePos.y);

    m_btnSimHovered = false;
    m_btnSettingsHovered = false;
    m_btnExitHovered = false;
    m_btnCampaignHovered = false;
    m_btnSandboxHovered = false;

    m_btnDetailedAtomsHovered = false;
    m_btnFxaaHovered = false;
    m_btnLanguageHovered = false;
    m_btnBackHovered = false;
    m_hoveredLevelIndex = -1;

    if (m_view == View::Main) {
        sf::FloatRect a, b, c;
        computeButtonRects(winSize, a, b, c);
        m_btnSimHovered = a.contains(m);
        m_btnSettingsHovered = b.contains(m);
        m_btnExitHovered = c.contains(m);
    }
    else if (m_view == View::ModeSelect) {
        m_btnCampaignHovered = computeModeButtonRect(winSize, 0).contains(m);
        m_btnSandboxHovered = computeModeButtonRect(winSize, 1).contains(m);
        m_btnBackHovered = computeModeBackRect(winSize).contains(m);
    }
    else if (m_view == View::LevelSelect) {
        m_hoveredLevelIndex = -1;
        for (int i = 0; i < LEVEL_COUNT; ++i) {
            if (computeLevelButtonRect(winSize, i).contains(m)) {
                m_hoveredLevelIndex = i;
                break;
            }
        }
        m_btnBackHovered = computeModeBackRect(winSize).contains(m);
    }
    else if (m_view == View::Settings) {
        m_btnDetailedAtomsHovered = computeSettingsRowRect(winSize, 0).contains(m);
        m_btnFxaaHovered = computeSettingsRowRect(winSize, 1).contains(m);
        m_btnLanguageHovered = computeSettingsRowRect(winSize, 2).contains(m);
        m_btnBackHovered = computeSettingsBackRect(winSize).contains(m);

        m_languageDropdownHovered = -1;
        if (m_languageDropdownOpen) {
            for (int i = 0; i < 3; ++i) {
                if (computeLanguageItemRect(winSize, i).contains(m)) {
                    m_languageDropdownHovered = i;
                    break;
                }
            }
        }
    }

    m_btnSkipHovered = computeSkipRect(winSize).contains(m);
    m_btnYtHovered = computeYtRect(winSize).contains(m);
}

int MainMenu::handleEvent(const sf::Event& event, sf::Vector2u winSize,
    std::mt19937& rng)
{
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            if (m_view == View::LevelSelect) {
                m_view = View::ModeSelect;
                return -1;
            }
            if (m_view == View::Settings && m_languageDropdownOpen) {
                m_languageDropdownOpen = false;
                return -1;
            }
            if (m_view == View::ModeSelect || m_view == View::Settings) {
                m_view = View::Main;
                return -1;
            }
        }
    }

    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mb->button == sf::Mouse::Button::Left) {
            sf::Vector2f m((float)mb->position.x, (float)mb->position.y);

            if (computeSkipRect(winSize).contains(m)) {
                restartDemo(rng);
                m_restartTimer = DEMO_RESTART_TIME;
                return -1;
            }

            if (computeYtRect(winSize).contains(m)) {
                openUrl(YT_URL);
                return -1;
            }

            if (m_view == View::Main) {
                sf::FloatRect a, b, c;
                computeButtonRects(winSize, a, b, c);
                if (a.contains(m)) { m_view = View::ModeSelect; return -1; }
                if (b.contains(m)) { m_view = View::Settings;   return -1; }
                if (c.contains(m)) return 2;
            }
            else if (m_view == View::ModeSelect) {
                if (computeModeBackRect(winSize).contains(m)) {
                    m_view = View::Main;
                    return -1;
                }
                if (computeModeButtonRect(winSize, 0).contains(m)) {
                    m_view = View::LevelSelect;
                    return -1;
                }
                if (computeModeButtonRect(winSize, 1).contains(m)) {
                    m_view = View::Main;
                    return 0;
                }
            }
            else if (m_view == View::LevelSelect) {
                if (computeModeBackRect(winSize).contains(m)) {
                    m_view = View::ModeSelect;
                    return -1;
                }
                if (computeLevelButtonRect(winSize, 0).contains(m)) return 3;
                if (computeLevelButtonRect(winSize, 1).contains(m)) return 4;
                if (computeLevelButtonRect(winSize, 2).contains(m)) return 5;
                if (computeLevelButtonRect(winSize, 3).contains(m)) return 6;

                // ← НОВОЕ: кнопка 6-го уровня (индекс 4)
                if (computeLevelButtonRect(winSize, 4).contains(m)) return 7;
            }
            else if (m_view == View::Settings) {
                // Сначала — пункты открытого дропдауна (три пункта).
                if (m_languageDropdownOpen) {
                    for (int i = 0; i < 3; ++i) {
                        if (computeLanguageItemRect(winSize, i).contains(m)) {
                            if (i == 0)      m_settings.language = Language::EN;
                            else if (i == 1) m_settings.language = Language::RU;
                            else             m_settings.language = Language::BROKEN;
                            m_languageDropdownOpen = false;
                            return -1;
                        }
                    }
                }

                if (computeSettingsRowRect(winSize, 0).contains(m)) {
                    m_settings.detailedAtoms = !m_settings.detailedAtoms;
                    m_languageDropdownOpen = false;
                    return -1;
                }
                if (computeSettingsRowRect(winSize, 1).contains(m)) {
                    m_settings.fxaaEnabled = !m_settings.fxaaEnabled;
                    m_languageDropdownOpen = false;
                    return -1;
                }
                if (computeSettingsRowRect(winSize, 2).contains(m)) {
                    m_languageDropdownOpen = !m_languageDropdownOpen;
                    return -1;
                }

                // Клик вне дропдауна и вне строки — закрываем.
                m_languageDropdownOpen = false;

                if (computeSettingsBackRect(winSize).contains(m)) {
                    m_view = View::Main;
                    return -1;
                }
            }
        }
    }
    return -1;
}

// ============================================================
// Рендер
// ============================================================

void MainMenu::render(sf::RenderWindow& window,
    const sf::Font& font, bool fontLoaded)
{
    sf::Vector2u winSize = window.getSize();
    float S = std::max(1.0f, (float)winSize.y / 1080.0f);

    window.clear(sf::Color(10, 12, 18));

    const float delta = DEMO_TEMP_C - TINT_NEUTRAL_C;
    float coldS = (delta < 0.0f)
        ? std::clamp(-delta / TINT_SATURATE_DELTA_C, 0.0f, 1.0f) : 0.0f;
    float hotS = (delta > 0.0f)
        ? std::clamp(delta / TINT_SATURATE_DELTA_C, 0.0f, 1.0f) : 0.0f;

    sf::Color vignetteColor = (coldS > 0.0f)
        ? sf::Color(70, 130, 220) : sf::Color(240, 120, 40);
    float vignetteIntensity = std::max(coldS, hotS) * TINT_MAX_ALPHA;

    sf::Color gridBaseColor = sf::Color(18, 18, 18);
    if (coldS > 0.0f) {
        gridBaseColor = lerpColor(sf::Color(18, 18, 18), sf::Color(22, 26, 37), coldS);
    }
    else if (hotS > 0.0f) {
        gridBaseColor = lerpColor(sf::Color(18, 18, 18), sf::Color(35, 24, 20), hotS);
    }

    // ---------- Демо-сцена ----------
    sf::View demoView;
    demoView.setViewport(sf::FloatRect(
        { DEMO_VP_LEFT, DEMO_VP_TOP },
        { DEMO_VP_WIDTH, DEMO_VP_HEIGHT }));

    float vpPixelW = DEMO_VP_WIDTH * (float)winSize.x;
    float vpPixelH = DEMO_VP_HEIGHT * (float)winSize.y;
    float vpAspect = vpPixelW / vpPixelH;

    const float pad = DEMO_BOX_SIZE + 2.0f;
    sf::Vector2f viewSize;
    if (vpAspect >= 1.0f) { viewSize.y = pad; viewSize.x = pad * vpAspect; }
    else { viewSize.x = pad; viewSize.y = pad / vpAspect; }
    demoView.setSize(viewSize);
    demoView.setCenter({ 0.0f, 0.0f });

    float worldPerPixel = viewSize.x / vpPixelW;
    const float half = DEMO_BOX_SIZE * 0.5f;

    window.setView(demoView);

    // Сетка
    {
        const float gridSpacing = 1.0f;
        const float pixelsPerCell = gridSpacing / worldPerPixel;
        float gridAlpha = std::clamp((pixelsPerCell - 2.0f) / 6.0f, 0.0f, 1.0f);
        gridAlpha = std::max(gridAlpha, 0.55f);

        sf::Color gridColor = gridBaseColor;
        gridColor.a = (std::uint8_t)(gridAlpha * 255.0f);

        sf::Vector2f vC = demoView.getCenter();
        sf::Vector2f vS = demoView.getSize();
        float gLeft = vC.x - vS.x * 0.5f;
        float gRight = vC.x + vS.x * 0.5f;
        float gTop = vC.y - vS.y * 0.5f;
        float gBottom = vC.y + vS.y * 0.5f;
        float startX = std::floor(gLeft / gridSpacing) * gridSpacing;
        float startY = std::floor(gTop / gridSpacing) * gridSpacing;

        sf::VertexArray lines(sf::PrimitiveType::Lines);
        for (float x = startX; x <= gRight; x += gridSpacing) {
            lines.append(sf::Vertex({ x, gTop }, gridColor));
            lines.append(sf::Vertex({ x, gBottom }, gridColor));
        }
        for (float y = startY; y <= gBottom; y += gridSpacing) {
            lines.append(sf::Vertex({ gLeft, y }, gridColor));
            lines.append(sf::Vertex({ gRight, y }, gridColor));
        }
        window.draw(lines);
    }

    // Рамка коробки
    {
        sf::RectangleShape boxShape({ DEMO_BOX_SIZE, DEMO_BOX_SIZE });
        boxShape.setOrigin({ half, half });
        boxShape.setPosition({ 0.0f, 0.0f });
        boxShape.setFillColor(sf::Color::Transparent);
        boxShape.setOutlineColor(sf::Color(140, 140, 140));
        boxShape.setOutlineThickness(2.0f * worldPerPixel);
        window.draw(boxShape);
    }

    // Связи
    {
        sf::VertexArray bonds(sf::PrimitiveType::Lines);
        const sf::Color colHH(204, 204, 255, 220);
        const sf::Color colOO(196, 68, 68, 230);
        const sf::Color colHO(220, 180, 180, 220);

        for (size_t i = 0; i < m_atoms.size(); ++i) {
            std::vector<int> partners;
            for (int j : m_atoms[i].bonds) {
                if (j <= (int)i) continue;
                bool already = false;
                for (int p : partners) if (p == j) { already = true; break; }
                if (!already) partners.push_back(j);
            }
            for (int j : partners) {
                int order = 0;
                for (int k : m_atoms[i].bonds) if (k == j) order++;
                sf::Color col = colHO;
                int e1 = m_atoms[i].elementId;
                int e2 = m_atoms[j].elementId;
                if (e1 == 1 && e2 == 1)      col = colOO;
                else if (e1 == 0 && e2 == 0) col = colHH;

                sf::Vector2f pi = m_atoms[i].pos;
                sf::Vector2f pj = m_atoms[j].pos;

                if (order >= 2) {
                    sf::Vector2f d = pj - pi;
                    float len = std::sqrt(d.x * d.x + d.y * d.y);
                    if (len > 1e-6f) {
                        sf::Vector2f n(-d.y / len, d.x / len);
                        float offset = m_atoms[i].radius * 0.45f;
                        bonds.append(sf::Vertex(pi + n * offset, col));
                        bonds.append(sf::Vertex(pj + n * offset, col));
                        bonds.append(sf::Vertex(pi - n * offset, col));
                        bonds.append(sf::Vertex(pj - n * offset, col));
                    }
                }
                else {
                    bonds.append(sf::Vertex(pi, col));
                    bonds.append(sf::Vertex(pj, col));
                }
            }
        }
        window.draw(bonds);
    }

    // Водородные связи
    {
        sf::VertexArray hbVA(sf::PrimitiveType::Lines);
        const sf::Color hbWater(140, 200, 240, 180);
        const int DASHES = 6;
        for (size_t i = 0; i < m_atoms.size(); ++i) {
            for (int j : m_atoms[i].hbonds) {
                if (j < 0 || j <= (int)i) continue;
                if (j >= (int)m_atoms.size()) continue;
                if (!isHBondedTo(m_atoms[j], (int)i)) continue;
                sf::Vector2f pi = m_atoms[i].pos;
                sf::Vector2f pj = m_atoms[j].pos;
                for (int s = 0; s < DASHES; s += 2) {
                    float t0 = (float)s / DASHES;
                    float t1 = (float)(s + 1) / DASHES;
                    hbVA.append(sf::Vertex(pi + (pj - pi) * t0, hbWater));
                    hbVA.append(sf::Vertex(pi + (pj - pi) * t1, hbWater));
                }
            }
        }
        window.draw(hbVA);
    }

    // Атомы
    for (const auto& a : m_atoms) {
        sf::CircleShape c(a.radius);
        c.setOrigin({ a.radius, a.radius });
        c.setPosition(a.pos);
        c.setFillColor(a.color);
        c.setOutlineColor(atomOutlineColor(a.elementId));
        c.setOutlineThickness(std::min(2.0f * worldPerPixel, a.radius * 0.5f));
        window.draw(c);
    }

    // ---------- Экранный слой ----------
    sf::View screenView(sf::FloatRect({ 0.0f, 0.0f },
        { (float)winSize.x, (float)winSize.y }));
    window.setView(screenView);

    drawVignette(window, winSize, vignetteColor, vignetteIntensity);

    // Оверлей
    if (m_view == View::Main) {
        float overlayLeft = (float)winSize.x * OVERLAY_LEFT_FRAC;
        sf::RectangleShape overlay({
            (float)winSize.x - overlayLeft, (float)winSize.y });
        overlay.setPosition({ overlayLeft, 0.0f });
        overlay.setFillColor(sf::Color(5, 8, 15, 175));
        window.draw(overlay);
    }
    else {
        sf::RectangleShape overlay({ (float)winSize.x, (float)winSize.y });
        overlay.setFillColor(sf::Color(0, 0, 0, 175));
        window.draw(overlay);
    }

    // Буквы элементов на атомах
    if (fontLoaded && !m_atoms.empty()) {
        sf::Text label(font, "H", 12);
        label.setStyle(sf::Text::Bold);
        label.setFillColor(sf::Color(30, 30, 30));

        for (const auto& a : m_atoms) {
            sf::Vector2i sp = window.mapCoordsToPixel(a.pos, demoView);
            if (sp.x < 0 || sp.x >(int)winSize.x ||
                sp.y < 0 || sp.y >(int)winSize.y) continue;

            float radiusPx = a.radius / worldPerPixel;
            int csize = std::max(8, (int)std::round(radiusPx * 1.4f));
            label.setCharacterSize(csize);
            label.setString(ATOM_TYPES[a.elementId].symbol);

            sf::FloatRect lb = label.getLocalBounds();
            label.setOrigin({ lb.position.x + lb.size.x * 0.5f,
                              lb.position.y + lb.size.y * 0.5f });
            label.setPosition({ (float)sp.x, (float)sp.y });
            window.draw(label);
        }
    }

    // ============================================================
    // Заголовок + подпись (не переводятся)
    // ============================================================
    const float titleBrightness =
        (m_view == View::ModeSelect || m_view == View::Settings ||
            m_view == View::LevelSelect)
        ? TITLE_DIM_MODESELECT : 1.0f;

    float rightCX = (float)winSize.x * BTN_CX_FRAC;
    if (fontLoaded) {
        int titleSize = (int)std::round(46.0f * S);
        sf::Text title(font, "H2 Fusion Simulation", titleSize);
        float maxTitleW = (float)winSize.x * 0.46f;
        while (title.getLocalBounds().size.x > maxTitleW && titleSize > 20) {
            titleSize -= 2;
            title.setCharacterSize(titleSize);
        }
        {
            sf::Color c(225, 235, 255);
            c.a = (std::uint8_t)std::clamp(255.0f * titleBrightness,
                0.0f, 255.0f);
            title.setFillColor(c);
        }
        title.setStyle(sf::Text::Bold);
        sf::FloatRect tb = title.getLocalBounds();
        title.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                         tb.position.y + tb.size.y * 0.5f });
        title.setPosition({ rightCX, (float)winSize.y * 0.22f });
        window.draw(title);

        int subSize = (int)std::round(15.0f * S);
        sf::Text sub(font,
            "This is a great story about a snake made entirely of oxygen.",
            subSize);
        float maxSubW = (float)winSize.x * 0.46f;
        while (sub.getLocalBounds().size.x > maxSubW && subSize > 10) {
            subSize -= 1;
            sub.setCharacterSize(subSize);
        }
        {
            sf::Color c(150, 165, 190);
            c.a = (std::uint8_t)std::clamp(255.0f * titleBrightness,
                0.0f, 255.0f);
            sub.setFillColor(c);
        }
        sf::FloatRect sb = sub.getLocalBounds();
        sub.setOrigin({ sb.position.x + sb.size.x * 0.5f,
                       sb.position.y + sb.size.y * 0.5f });
        sub.setPosition({ rightCX, (float)winSize.y * 0.30f });
        window.draw(sub);
    }

    // ============================================================
    // Кнопки, специфичные для текущего View
    // ============================================================
    if (m_view == View::Main) {
        sf::FloatRect r0, r1, r2;
        computeButtonRects(winSize, r0, r1, r2);
        drawButton(window, font, fontLoaded, r0,
            tr(Loc::BtnSimulation, m_settings.language), m_btnSimHovered, S);
        drawButton(window, font, fontLoaded, r1,
            tr(Loc::BtnSettings, m_settings.language), m_btnSettingsHovered, S);
        drawButton(window, font, fontLoaded, r2,
            tr(Loc::BtnExit, m_settings.language), m_btnExitHovered, S);
    }
    else if (m_view == View::Settings) {
        if (fontLoaded) {
            int titleSize = (int)std::round(40.0f * S);
            sf::Text title(font,
                tr(Loc::BtnSettings, m_settings.language), titleSize);
            title.setFillColor(sf::Color(225, 235, 255));
            title.setStyle(sf::Text::Bold);
            sf::FloatRect tb = title.getLocalBounds();
            title.setOrigin({ tb.position.x + tb.size.x * 0.5f,
                              tb.position.y + tb.size.y * 0.5f });
            title.setPosition({ (float)winSize.x * 0.5f,
                                (float)winSize.y * SETTINGS_HEADING_Y_FRAC });
            window.draw(title);
        }

        drawSettingsRow(window, font, fontLoaded,
            computeSettingsRowRect(winSize, 0),
            tr(Loc::DetailedAtoms, m_settings.language),
            m_settings.detailedAtoms, m_btnDetailedAtomsHovered, S);

        drawSettingsRow(window, font, fontLoaded,
            computeSettingsRowRect(winSize, 1),
            tr(Loc::Fxaa, m_settings.language),
            m_settings.fxaaEnabled, m_btnFxaaHovered, S);

        drawLanguageRow(window, font, fontLoaded,
            computeSettingsRowRect(winSize, 2),
            m_settings.language, m_languageDropdownOpen,
            m_languageDropdownHovered, m_btnLanguageHovered, S);

        drawButton(window, font, fontLoaded,
            computeSettingsBackRect(winSize),
            tr(Loc::BtnBack, m_settings.language), m_btnBackHovered, S);

        if (fontLoaded) {
            int hintSize = (int)std::round(13.0f * S);
            sf::Text hint(font, tr(Loc::EscBack, m_settings.language), hintSize);
            hint.setFillColor(sf::Color(150, 165, 190));
            sf::FloatRect hb = hint.getLocalBounds();
            hint.setOrigin({ hb.position.x + hb.size.x * 0.5f,
                             hb.position.y + hb.size.y * 0.5f });
            hint.setPosition({ (float)winSize.x * 0.5f,
                                (float)winSize.y - 32.0f * S });
            window.draw(hint);
        }
    }
    else if (m_view == View::ModeSelect) {
        if (fontLoaded) {
            float iconTopY = computeModeIconTopY(winSize);
            float headingY = iconTopY - MODE_HEADING_MARGIN_SCALE * S;
            if (headingY < 20.0f * S) headingY = 20.0f * S;

            int headingSize = (int)std::round(22.0f * S);
            sf::Text heading(font,
                tr(Loc::SelectMode, m_settings.language), headingSize);
            heading.setFillColor(sf::Color(180, 195, 220));
            heading.setStyle(sf::Text::Bold);
            sf::FloatRect hb = heading.getLocalBounds();
            heading.setOrigin({ hb.position.x + hb.size.x * 0.5f,
                               hb.position.y + hb.size.y * 0.5f });
            heading.setPosition({ (float)winSize.x * 0.5f, headingY });
            window.draw(heading);
        }

        sf::FloatRect campRect = computeModeButtonRect(winSize, 0);
        sf::FloatRect sandRect = computeModeButtonRect(winSize, 1);

        drawModeButton(window, font, fontLoaded, campRect,
            tr(Loc::BtnCampaign, m_settings.language),
            m_btnCampaignHovered, S, 0,
            &m_campaignTexture, m_campaignLoaded,
            nullptr, false);
        drawModeButton(window, font, fontLoaded, sandRect,
            tr(Loc::BtnSandbox, m_settings.language),
            m_btnSandboxHovered, S, 1,
            nullptr, false,
            &m_sandboxTexture, m_sandboxLoaded);

        drawButton(window, font, fontLoaded,
            computeModeBackRect(winSize),
            tr(Loc::BtnBack, m_settings.language), m_btnBackHovered, S);
    }
    else {
        if (fontLoaded) {
            int headingSize = (int)std::round(26.0f * S);
            sf::Text heading(font,
                tr(Loc::SelectLevel, m_settings.language), headingSize);
            heading.setFillColor(sf::Color(180, 195, 220));
            heading.setStyle(sf::Text::Bold);
            sf::FloatRect hb = heading.getLocalBounds();
            heading.setOrigin({ hb.position.x + hb.size.x * 0.5f,
                               hb.position.y + hb.size.y * 0.5f });
            heading.setPosition({ (float)winSize.x * 0.5f,
                                  (float)winSize.y * LEVEL_HEADING_Y_FRAC });
            window.draw(heading);
        }

        sf::FloatRect levelRect0 = computeLevelButtonRect(winSize, 0);
        drawLevelButton(window, font, fontLoaded, levelRect0,
            1,
            tr(Loc::Level1Title, m_settings.language),
            tr(Loc::Level1Subtitle, m_settings.language),
            m_hoveredLevelIndex == 0, S);

        sf::FloatRect levelRect1 = computeLevelButtonRect(winSize, 1);
        drawLevelButton(window, font, fontLoaded, levelRect1,
            2,
            tr(Loc::Level2Title, m_settings.language),
            tr(Loc::Level2Subtitle, m_settings.language),
            m_hoveredLevelIndex == 1, S);

        sf::FloatRect levelRect2 = computeLevelButtonRect(winSize, 2);
        drawLevelButton(window, font, fontLoaded, levelRect2,
            3,
            tr(Loc::Level3Title, m_settings.language),
            tr(Loc::Level3Subtitle, m_settings.language),
            m_hoveredLevelIndex == 2, S);

        sf::FloatRect levelRect3 = computeLevelButtonRect(winSize, 3);
        drawLevelButton(window, font, fontLoaded, levelRect3,
            4,
            tr(Loc::Level4Title, m_settings.language),
            tr(Loc::Level4Subtitle, m_settings.language),
            m_hoveredLevelIndex == 3, S);

        // ← НОВОЕ: 6-й уровень
        sf::FloatRect levelRect4 = computeLevelButtonRect(winSize, 4);
        drawLevelButton(window, font, fontLoaded, levelRect4,
            6,
            tr(Loc::Level6Title, m_settings.language),
            tr(Loc::Level6Subtitle, m_settings.language),
            m_hoveredLevelIndex == 4, S);

        drawButton(window, font, fontLoaded,
            computeModeBackRect(winSize),
            tr(Loc::BtnBack, m_settings.language), m_btnBackHovered, S);
    }

    // Skip-кнопка ►► — только в Main view.
    if (m_view == View::Main) {
        drawSkipButton(window, computeSkipRect(winSize), m_btnSkipHovered, S);
    }

    // YouTube — всегда.
    drawYtIcon(window, computeYtRect(winSize), m_btnYtHovered,
        &m_ytTexture, m_ytLoaded, S);
}