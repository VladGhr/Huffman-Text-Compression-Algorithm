#include <QGuiApplication>
#include <QApplication>
#include <QFileDialog>
#include <QPushButton>
#include <QLineEdit>
#include <QScreen>
#include <QFont>
#include <QIcon>
#include <QDir>
#include "utils.h"

#define OCD_OFFSET 1.5f

QString open_explorer(QWidget *parent)
{
    QString filePath = QFileDialog::getOpenFileName(parent, "Select file", QDir::homePath(), "Text (*.txt)");
    return filePath;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    QLineEdit *compressed_file_box = new QLineEdit(&window);
    QLineEdit *decompressed_file_box = new QLineEdit(&window);

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect geo = screen->geometry();
    QPushButton *compress_button = new QPushButton("Select file for compression", &window);
    QPushButton *decompress_button = new QPushButton("Select file for decompression", &window);

    int button_width = compress_button->size().width() * 2;
    int button_height = compress_button->size().height() * 1.5;

    int screen_width = geo.width();
    int screen_height = geo.height();
    
    int compress_button_x = screen_width / 4 - button_width * 2;
    int compress_button_y = screen_height / 4 - button_height / 2;

    int decompress_button_x = screen_width / 4 + button_width;
    int decompress_button_y = screen_height / 4 - button_height / 2;

    window.setWindowTitle("Huffman Compressor");
    window.resize(screen_width / 2, screen_height / 2);


    decompress_button->resize(button_width, button_height);
    decompress_button->move(decompress_button_x, decompress_button_y);
    decompressed_file_box->move(decompress_button_x + OCD_OFFSET, decompress_button_y + button_height);
    decompressed_file_box->setPlaceholderText("No file selected");
    decompressed_file_box->setReadOnly(true);


    compress_button->resize(button_width, button_height);
    compress_button->move(compress_button_x, compress_button_y);
    compressed_file_box->move(compress_button_x + OCD_OFFSET, compress_button_y + button_height);
    compressed_file_box->setPlaceholderText("No file selected");
    compressed_file_box->setReadOnly(true);

    QString file_to_compress;
    QObject::connect(compress_button, &QPushButton::clicked, [&window, &file_to_compress, compressed_file_box](){ 
                        QString path = open_explorer(&window); 
                        if(!path.isNull()) {
                            file_to_compress = path;
                            compressed_file_box->setText(file_to_compress);
                        }
    });
    


    window.show();
    return app.exec();
}