#include <QtCore/QObject>
#include <QtWidgets/QWidget>

class Test : public QObject // Warn
{
public:
    Test();
};

class Test2 : public Test // Warn
{
public:
    Test2();
};

class Test3 // OK
{
public:
    Test3();
};

class Test4 : public Test // OK
{
public:
    Test4();
    Test4(QObject*); //  Not common, but lets not warn
};

class Test5 : public Test // Warn
{
public:
    Test5(const QObject*);
};

class QObject; // OK

class WTest : public QWidget
{
public:
    WTest(QWidget *); // OK
};

class WTest2 : public QWidget
{
public:
    WTest2(QObject *); // Warn
};

namespace Qt3DCore {
    class QNode : public QObject {};
    // This is just a dummy so we don't have to depend on Qt3D
    class QEntity : public QNode // clazy:exclude=ctor-missing-parent-argument
    {
    };
}

struct MyEntity : Qt3DCore::QEntity // Warn
{
    MyEntity();
};

struct MyEntity2 : Qt3DCore::QEntity { // OK
    MyEntity2(Qt3DCore::QNode*);
};

namespace Qt3DCore
{
    struct MyEntity3 : QEntity { // OK
    MyEntity3(QNode*);
};
}
