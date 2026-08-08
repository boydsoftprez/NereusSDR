#include <QTest>
#include <QFile>
#include <QRegularExpression>
#include <QString>

#include "gui/DssMeshGeometry.h"

using namespace NereusSDR;

namespace {

QString shaderSource(const QString& name)
{
    // Tests run from the build dir; the sources live in the source tree.
    QFile f(QStringLiteral(NEREUS_SOURCE_DIR "/resources/shaders/") + name);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll());
}

int declaredRowFramesLength(const QString& src)
{
    static const QRegularExpression re(
        QStringLiteral(R"(vec4\s+rowFrames\s*\[\s*(\d+)\s*\])"));
    const auto m = re.match(src);
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

}  // namespace

class TestDssShaderContract : public QObject {
    Q_OBJECT

private slots:
    void shaders_exist() {
        QVERIFY(!shaderSource(QStringLiteral("dss_mesh.vert")).isEmpty());
        QVERIFY(!shaderSource(QStringLiteral("dss_mesh.frag")).isEmpty());
    }

    // Fixed GLSL array sizes cannot consume a C++ constant. Upstream guards
    // this with a static_assert; we assert it in a test so a change to
    // kDssRows cannot silently overrun the UBO.
    void rowFramesArray_matchesRingRowCount() {
        QCOMPARE(declaredRowFramesLength(
                     shaderSource(QStringLiteral("dss_mesh.vert"))), kDssRows);
        QCOMPARE(declaredRowFramesLength(
                     shaderSource(QStringLiteral("dss_mesh.frag"))), kDssRows);
    }

    // std140 rounds the scalar run up to a vec4 boundary. The host writes
    // explicit padding to match; if this drifts, every vec4 after it shifts.
    void uboFloatCount_matchesStd140Layout() {
        // 22 scalars, padded to 24, then bgFill(4) + shadowBands(8*4)
        // + shadowStyles(8*4) + shadowMeta(4) + rowFrames(kDssRows*4).
        const int expected = 24 + 4 + 32 + 32 + 4 + kDssRows * 4;
        QCOMPARE(kDssMeshUboFloats, expected);
    }

    // Upstream issue annotations are load-bearing history and no script
    // enforces their survival (design doc section 9.1).
    void upstreamIssueAnnotations_survivedThePort() {
        const QString vert = shaderSource(QStringLiteral("dss_mesh.vert"));
        const QString frag = shaderSource(QStringLiteral("dss_mesh.frag"));
        QVERIFY2(vert.contains(QStringLiteral("rowSpanFactor")),
                 "vertex shader lost the row-span widening path");
        QVERIFY2(frag.contains(QStringLiteral("applySliceShadow")),
                 "fragment shader lost the slice shadow decal path");
    }
};

QTEST_APPLESS_MAIN(TestDssShaderContract)
#include "tst_dss_shader_contract.moc"
