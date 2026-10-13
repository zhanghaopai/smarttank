#include <QApplication>
#include <QLabel>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Standard_Version.hxx>
#include <iostream>

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QLabel label(QStringLiteral("SmartTank dependency check"));
    const auto cylinder = BRepPrimAPI_MakeCylinder(1000.0, 2000.0).Shape();
    if (cylinder.IsNull() || label.text().isEmpty()) return 1;
    std::cout << "Qt " << qVersion() << "; OCCT " << OCC_VERSION_COMPLETE << '\n';
    return 0;
}
