#pragma once

#include <QColor>
#include <QObject>

namespace app {

// Palette alignée sur l'app web d'origine (bingo-app/public/style/main.css).
class Theme : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool dark READ dark NOTIFY paletteChanged)

    Q_PROPERTY(QColor background  READ background  NOTIFY paletteChanged)
    Q_PROPERTY(QColor surface     READ surface     NOTIFY paletteChanged)
    Q_PROPERTY(QColor surfaceHigh READ surfaceHigh NOTIFY paletteChanged)
    Q_PROPERTY(QColor inputBg     READ inputBg     NOTIFY paletteChanged)
    Q_PROPERTY(QColor outline     READ outline     NOTIFY paletteChanged)
    Q_PROPERTY(QColor outlineLight READ outlineLight NOTIFY paletteChanged)
    Q_PROPERTY(QColor accent      READ accent      NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentDim   READ accentDim   NOTIFY paletteChanged)
    Q_PROPERTY(QColor accentSoft  READ accentSoft  NOTIFY paletteChanged)
    Q_PROPERTY(QColor onAccent    READ onAccent    NOTIFY paletteChanged)
    Q_PROPERTY(QColor text        READ text        NOTIFY paletteChanged)
    Q_PROPERTY(QColor textDim     READ textDim     NOTIFY paletteChanged)
    Q_PROPERTY(QColor danger      READ danger      NOTIFY paletteChanged)
    Q_PROPERTY(QColor warning     READ warning     NOTIFY paletteChanged)
    Q_PROPERTY(QColor success     READ success     NOTIFY paletteChanged)

    Q_PROPERTY(int radius      MEMBER m_radius      CONSTANT)
    Q_PROPERTY(int radiusLg    MEMBER m_radiusLg    CONSTANT)
    Q_PROPERTY(int touchTarget MEMBER m_touchTarget CONSTANT)
    Q_PROPERTY(int gap         MEMBER m_gap         CONSTANT)
    Q_PROPERTY(int pad         MEMBER m_pad         CONSTANT)
    Q_PROPERTY(int contentMax  MEMBER m_contentMax  CONSTANT)

public:
    explicit Theme(QObject *parent = nullptr);

    bool dark() const { return m_dark; }

    // Identité visuelle bingo : fond bleu nuit, accent indigo.
    QColor background()    const { return QColor("#0f1623"); }
    QColor surface()       const { return QColor("#1a2235"); }
    QColor surfaceHigh()   const { return QColor("#232f45"); }
    QColor inputBg()       const { return QColor("#111827"); }
    QColor outline()       const { return QColor("#2a3a52"); }
    QColor outlineLight()  const { return QColor("#3d5270"); }
    QColor accent()        const { return QColor("#6366f1"); }
    QColor accentDim()     const { return QColor("#4f46e5"); }
    QColor accentSoft()    const { return QColor(99, 102, 241, 38); }
    QColor onAccent()      const { return QColor("#ffffff"); }
    QColor text()          const { return QColor("#f1f5f9"); }
    QColor textDim()       const { return QColor("#7c8fa6"); }
    QColor danger()        const { return QColor("#ef4444"); }
    QColor warning()       const { return QColor("#f59e0b"); }
    QColor success()       const { return QColor("#22c55e"); }

signals:
    void paletteChanged();

private:
    void applyColorScheme();

    bool m_dark = true;

    int m_radius      = 8;
    int m_radiusLg    = 12;
    int m_touchTarget = 44;
    int m_gap         = 10;
    int m_pad         = 16;
    int m_contentMax  = 680;
};

} // namespace app
