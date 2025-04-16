#include "test.hpp"
#include "test2.hpp"
TestWidget::TestWidget(QWidget* parent) :QDockWidget("Sheila and Jose plugin test",parent){
    this->parent=parent;

    QWidget *widget= new QWidget();
    this->button->setText("Press me if you love Miami!!");
    QHBoxLayout *layout= new QHBoxLayout();
    layout->addWidget(this->button);

    QLabel *label = new QLabel("Dock del plugin con cuadrado blanco", this);
    layout->addWidget(label);


    WhiteSquareWidget *square = new WhiteSquareWidget(this);
    layout->addWidget(square);


    setObjectName("WhiteSquareDock");  // Necesario para que OBS lo registre como dock


    widget->setLayout(layout);

    setWidget(widget);
    setVisible(false);
    setFloating(true);
    resize(300,300);

    QObject::connect(button,SIGNAL(clicked()),SLOT(ButtonClicked()));


    
    //QVBoxLayout *layout = new QVBoxLayout(this);

  

    

};

TestWidget::~TestWidget(){}

void TestWidget::ButtonClicked(){
    QMessageBox::information(this, "Information Box", "RACIST!");
}