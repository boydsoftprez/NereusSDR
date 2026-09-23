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
    // explicit padding to match; if this drifts, every vec4 after it shifts
    // and the surface renders garbage with no compile error.
    //
    // This MUST parse the real uniform block out of the shader. Restating
    // kDssMeshUboFloats' own arithmetic here would be a tautology: the two
    // expressions share kDssRows and constant-fold identically, so they can
    // never disagree, and an edit to the GLSL block would sail through. That
    // matters most for the task that writes this UBO, which is the one most
    // likely to change the block.
    void uboFloatCount_matchesStd140Layout() {
        const QString src = shaderSource(QStringLiteral("dss_mesh.vert"));
        QVERIFY(!src.isEmpty());
        static const QRegularExpression blockRe(
            QStringLiteral(R"(layout\(std140[^{]*\{(.*?)\n\};)"),
            QRegularExpression::DotMatchesEverythingOption);
        const auto blockMatch = blockRe.match(src);
        QVERIFY2(blockMatch.hasMatch(), "no std140 uniform block found");
        // Strip comments so a commented-out member is not counted.
        static const QRegularExpression commentRe(QStringLiteral("//[^\n]*"));
        const QString body =
            blockMatch.captured(1).remove(commentRe);

        static const QRegularExpression memberRe(
            QStringLiteral(R"(\b(float|vec4)\s+\w+\s*(?:\[\s*(\d+)\s*\])?\s*;)"));
        int scalars = 0;
        int vec4Slots = 0;
        bool seenVec4 = false;
        auto it = memberRe.globalMatch(body);
        while (it.hasNext()) {
            const auto m = it.next();
            if (m.captured(1) == QLatin1String("float")) {
                // std140 packing here assumes every scalar precedes every
                // vec4. If that ever stops being true the padding maths below
                // is wrong, so fail loudly rather than compute a wrong total.
                QVERIFY2(!seenVec4,
                         "a float is declared after a vec4; std140 padding "
                         "assumption in this test no longer holds");
                ++scalars;
            } else {
                seenVec4 = true;
                const QString count = m.captured(2);
                vec4Slots += count.isEmpty() ? 1 : count.toInt();
            }
        }
        QVERIFY2(scalars > 0 && vec4Slots > 0, "uniform block parse found nothing");
        const int paddedScalars = ((scalars + 3) / 4) * 4;
        QCOMPARE(kDssMeshUboFloats, paddedScalars + vec4Slots * 4);
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
