#pragma once
#include "Types.hpp"
#include "Localization.hpp"
#include <SFML/Graphics.hpp>
#include <random>
#include <vector>

// Глобальное состояние приложения
enum class GameState {
    MainMenu,
    Simulation
};

// Главное меню: фоновая демо-симуляция + кнопки.
// Есть два вида панели:
//   Main       — Simulation / Settings / Exit (кнопки справа)
//   ModeSelect — Campaign / Sandbox (квадратные кнопки по центру,
//                на полупрозрачном оверлее, с геом. иконками)
// Sandbox → возвращает 0 (переход в основную симуляцию).
// Также в углу — иконка YouTube (youtubee.png), клик открывает канал.
class MainMenu {
public:
    enum class View {
        Main,
        ModeSelect,
        LevelSelect,      // ← список уровней внутри Campaign
        Settings
    };

    // Настройки, которые транслируются в симуляцию.
    struct Settings {
        bool detailedAtoms = true;   // ядро + почернение при зуме
        bool fxaaEnabled = false;  // сглаживание FXAA
        Language language = Language::EN;  // язык интерфейса
    };

    const Settings& getSettings() const { return m_settings; }
    void setSettings(const Settings& s) { m_settings = s; }

    MainMenu();

    void init(std::mt19937& rng);

    void updateHover(sf::Vector2i mousePos, sf::Vector2u winSize);
    void update(float dt, std::mt19937& rng);
    void render(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded);

    // Возвращает: -1 = ничего, 0 = Simulation, 1 = Settings, 2 = Exit
    int handleEvent(const sf::Event& event, sf::Vector2u winSize,
        std::mt19937& rng);

private:
    void restartDemo(std::mt19937& rng);
    void stepDemoPhysics(std::mt19937& rng);

    std::vector<Atom>  m_atoms;
    SpatialGrid        m_grid;

    float m_restartTimer = 45.0f;
    View  m_view = View::Main;

    // Main view
    bool m_btnSimHovered = false;
    bool m_btnSettingsHovered = false;
    bool m_btnExitHovered = false;

    // ModeSelect view
    bool m_btnCampaignHovered = false;
    bool m_btnSandboxHovered = false;

    // LevelSelect view
    int m_hoveredLevelIndex = -1;   // -1 = ни одна кнопка уровня не под курсором

    // Общие (рисуются поверх обоих View)
    bool m_btnSkipHovered = false;
    bool m_btnYtHovered = false;

    // YouTube-иконка
    sf::Texture m_ytTexture;
    bool        m_ytLoaded = false;

    // Campaign-иконка (Campaign.jpg)
    sf::Texture m_campaignTexture;
    bool        m_campaignLoaded = false;

    // Sandbox-иконка (Sandbox.jpg)
    sf::Texture m_sandboxTexture;
    bool        m_sandboxLoaded = false;

    // Settings view
    Settings m_settings;
    bool m_btnDetailedAtomsHovered = false;
    bool m_btnFxaaHovered = false;
    bool m_btnLanguageHovered = false;
    bool m_btnBackHovered = false;

    // Выпадающий список языка
    bool m_languageDropdownOpen = false;
    int  m_languageDropdownHovered = -1;   // -1 / 0=EN / 1=RU
};