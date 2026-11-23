#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <stdexcept>

#include "compression.h"
#include "decompression.h"

// ---------------------------------------------------------------------------
// Drop zone: a dashed box that accepts a single file by drag and drop,
// with a "Browse" button inside that opens the file explorer.
// ---------------------------------------------------------------------------
class DropZone : public QFrame
{
public:
    std::function<void(const QString &)> on_file_selected;

    explicit DropZone(QWidget *parent = nullptr) : QFrame(parent)
    {
        setObjectName("dropZone");
        setAcceptDrops(true);
        setMinimumHeight(200);

        icon_label = new QLabel(this);
        icon_label->setAlignment(Qt::AlignCenter);
        icon_label->setPixmap(style()->standardIcon(QStyle::SP_FileIcon).pixmap(48, 48));

        title_label = new QLabel(this);
        title_label->setObjectName("dropTitle");
        title_label->setAlignment(Qt::AlignCenter);
        title_label->setWordWrap(true);

        detail_label = new QLabel(this);
        detail_label->setObjectName("dropDetail");
        detail_label->setAlignment(Qt::AlignCenter);
        detail_label->setWordWrap(true);

        browse_button = new QPushButton("Browse...", this);
        browse_button->setObjectName("browseButton");
        browse_button->setCursor(Qt::PointingHandCursor);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 24, 24, 24);
        layout->setSpacing(8);
        layout->addStretch();
        layout->addWidget(icon_label);
        layout->addWidget(title_label);
        layout->addWidget(detail_label);
        layout->addSpacing(8);
        layout->addWidget(browse_button, 0, Qt::AlignCenter);
        layout->addStretch();

        QObject::connect(browse_button, &QPushButton::clicked, this, [this]() {
            QString start_dir = current_file.isEmpty() ? QDir::homePath() : QFileInfo(current_file).absolutePath();
            QString path = QFileDialog::getOpenFileName(this, "Select file", start_dir,
                                                        "All files (*);;Huffman archives (*.huff)");
            if (!path.isEmpty())
                select_file(path);
        });

        show_empty();
    }

    void select_file(const QString &path)
    {
        QFileInfo info(path);
        current_file = info.absoluteFilePath();

        title_label->setText(info.fileName());

        // single line: size · folder (long folders are shortened in the middle, full path in the tooltip)
        QString size_text = QLocale().formattedDataSize(info.size()) + "   ·   ";
        QString folder = QDir::toNativeSeparators(info.absolutePath());
        int available = qMax(120, width() - 64 - detail_label->fontMetrics().horizontalAdvance(size_text));
        detail_label->setWordWrap(false);
        detail_label->setText(size_text + detail_label->fontMetrics().elidedText(folder, Qt::ElideMiddle, available));
        detail_label->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
        browse_button->setText("Change file...");

        if (on_file_selected)
            on_file_selected(current_file);
    }

    QString file() const { return current_file; }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (single_local_file(event->mimeData()).isEmpty())
            return;

        event->acceptProposedAction();
        set_drag_active(true);
    }

    void dragLeaveEvent(QDragLeaveEvent *) override
    {
        set_drag_active(false);
    }

    void dropEvent(QDropEvent *event) override
    {
        set_drag_active(false);

        QString path = single_local_file(event->mimeData());
        if (path.isEmpty())
            return;

        event->acceptProposedAction();
        select_file(path);
    }

private:
    QLabel *icon_label;
    QLabel *title_label;
    QLabel *detail_label;
    QPushButton *browse_button;
    QString current_file;

    void show_empty()
    {
        title_label->setText("Drag and drop a file here");
        detail_label->setText("any file to compress, or a .huff file to decompress");
    }

    void set_drag_active(bool active)
    {
        setProperty("dragActive", active);
        style()->unpolish(this);
        style()->polish(this);
    }

    // returns the path if exactly one local regular file is dragged, otherwise ""
    static QString single_local_file(const QMimeData *mime)
    {
        if (mime == nullptr || !mime->hasUrls() || mime->urls().size() != 1)
            return QString();

        QUrl url = mime->urls().first();
        if (!url.isLocalFile())
            return QString();

        QString path = url.toLocalFile();
        return QFileInfo(path).isFile() ? path : QString();
    }
};

// ---------------------------------------------------------------------------

static const char *STYLE_SHEET = R"(
QWidget#window {
    background: #f6f7f9;
}
QLabel {
    color: #1f2933;
}
QLabel#appTitle {
    font-size: 20px;
    font-weight: 600;
}
QLabel#appSubtitle {
    color: #616e7c;
}
QFrame#dropZone {
    background: #ffffff;
    border: 2px dashed #b8c2cc;
    border-radius: 12px;
}
QFrame#dropZone[dragActive="true"] {
    background: #eef4ff;
    border-color: #3a6ff7;
}
QLabel#dropTitle {
    font-size: 15px;
    font-weight: 600;
}
QLabel#dropDetail {
    color: #616e7c;
}
QPushButton {
    padding: 8px 18px;
    border-radius: 8px;
    border: 1px solid #c5ccd3;
    background: #ffffff;
    color: #1f2933;
}
QPushButton:hover {
    background: #f0f2f5;
}
QPushButton:disabled {
    color: #9aa5b1;
    background: #f0f2f5;
    border-color: #e1e5ea;
}
QPushButton#actionButton {
    padding: 12px 18px;
    font-size: 14px;
    font-weight: 600;
    background: #3a6ff7;
    border-color: #3a6ff7;
    color: #ffffff;
}
QPushButton#actionButton:hover {
    background: #2f5ee0;
}
QPushButton#actionButton:disabled {
    background: #c9d6fb;
    border-color: #c9d6fb;
    color: #ffffff;
}
QLabel#status {
    padding: 10px 12px;
    border-radius: 8px;
    background: #ffffff;
    border: 1px solid #e1e5ea;
}
QLabel#status[kind="success"] {
    background: #ecfdf3;
    border-color: #a6e3bf;
    color: #146c3b;
}
QLabel#status[kind="error"] {
    background: #fef1f1;
    border-color: #f5b5b5;
    color: #a12121;
}
QProgressBar {
    border: none;
    background: #e1e5ea;
    border-radius: 3px;
    max-height: 6px;
}
QProgressBar::chunk {
    background: #3a6ff7;
    border-radius: 3px;
}
)";

static void set_status(QLabel *status, const QString &text, const QString &kind)
{
    status->setText(text);
    status->setProperty("kind", kind);
    status->style()->unpolish(status);
    status->style()->polish(status);
    status->setVisible(!text.isEmpty());
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setObjectName("window");
    window.setWindowTitle("Huffman Compressor");
    window.setStyleSheet(STYLE_SHEET);
    // no explicit minimum size: the layout computes it, so the window grows
    // instead of squeezing the drop zone when the status message appears
    window.resize(560, 560);

    QString icon_path = QCoreApplication::applicationDirPath() + "/../icons/zip_file_image.png";
    if (QFileInfo::exists(icon_path))
        window.setWindowIcon(QIcon(icon_path));

    // header
    QLabel *title = new QLabel("Huffman Compressor");
    title->setObjectName("appTitle");
    QLabel *subtitle = new QLabel("Compress any file into a .huff archive, or restore the original from one.");
    subtitle->setObjectName("appSubtitle");
    subtitle->setWordWrap(true);

    // drop zone with Browse button
    DropZone *drop_zone = new DropZone;

    // action buttons
    QPushButton *compress_button = new QPushButton("Compress");
    QPushButton *decompress_button = new QPushButton("Decompress");
    for (QPushButton *b : {compress_button, decompress_button})
    {
        b->setObjectName("actionButton");
        b->setCursor(Qt::PointingHandCursor);
        b->setEnabled(false);
    }
    compress_button->setToolTip("Create a .huff archive next to the selected file");
    decompress_button->setToolTip("Restore the original file from a .huff archive");

    QHBoxLayout *actions = new QHBoxLayout;
    actions->setSpacing(12);
    actions->addWidget(compress_button);
    actions->addWidget(decompress_button);

    // progress + status
    QProgressBar *progress = new QProgressBar;
    progress->setRange(0, 0); // indeterminate
    progress->setTextVisible(false);
    progress->setVisible(false);

    QLabel *status = new QLabel;
    status->setObjectName("status");
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status->setVisible(false);

    QPushButton *open_folder_button = new QPushButton("Show in folder");
    open_folder_button->setCursor(Qt::PointingHandCursor);
    open_folder_button->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(drop_zone, 1);
    layout->addLayout(actions);
    layout->addWidget(progress);
    layout->addWidget(status);
    layout->addWidget(open_folder_button, 0, Qt::AlignLeft);

    QString last_output;
    bool busy = false;

    auto update_buttons = [&]() {
        QString file = drop_zone->file();
        bool has_file = !file.isEmpty();
        bool is_huff = file.endsWith(".huff", Qt::CaseInsensitive);

        compress_button->setEnabled(!busy && has_file);
        // only .huff files can be decompressed
        decompress_button->setEnabled(!busy && has_file && is_huff);
        drop_zone->setEnabled(!busy);
    };

    drop_zone->on_file_selected = [&](const QString &) {
        set_status(status, "", "");
        open_folder_button->setVisible(false);
        update_buttons();
    };

    // runs compress()/decompress() on a worker thread so the window stays responsive
    auto run = [&](bool compressing) {
        QString input = drop_zone->file();
        if (input.isEmpty())
            return;

        busy = true;
        update_buttons();
        progress->setVisible(true);
        open_folder_button->setVisible(false);
        set_status(status, compressing ? "Compressing..." : "Decompressing...", "");

        std::string input_utf8 = input.toStdString();

        QThread *worker = QThread::create([&window, &busy, &last_output, update_buttons, progress, status,
                                           open_folder_button, compressing, input_utf8]() {
            QString result_text;
            QString kind;
            QString output;

            try
            {
                std::string out = compressing ? compress(input_utf8) : decompress(input_utf8);
                output = QString::fromStdString(out);

                qint64 in_size = QFileInfo(QString::fromStdString(input_utf8)).size();
                qint64 out_size = QFileInfo(output).size();
                QLocale locale;

                result_text = QString("%1 %2\n%3  →  %4")
                                  .arg(compressing ? "Compressed to" : "Decompressed to")
                                  .arg(QDir::toNativeSeparators(output))
                                  .arg(locale.formattedDataSize(in_size))
                                  .arg(locale.formattedDataSize(out_size));

                if (compressing && in_size > 0)
                    result_text += QString("  (%1% of the original)").arg(100.0 * out_size / in_size, 0, 'f', 1);

                kind = "success";
            }
            catch (const std::exception &e)
            {
                result_text = QString("Error: ") + QString::fromStdString(e.what());
                kind = "error";
            }

            // back on the UI thread
            QMetaObject::invokeMethod(&window, [&busy, &last_output, update_buttons, progress, status,
                                                open_folder_button, result_text, kind, output]() {
                busy = false;
                progress->setVisible(false);
                set_status(status, result_text, kind);
                last_output = output;
                open_folder_button->setVisible(kind == "success");
                update_buttons();
            }, Qt::QueuedConnection);
        });

        QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        // if the window is closed mid-operation, let the worker finish before exiting
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, worker, [worker]() { worker->wait(); });
        worker->start();
    };

    QObject::connect(compress_button, &QPushButton::clicked, [&]() { run(true); });
    QObject::connect(decompress_button, &QPushButton::clicked, [&]() { run(false); });
    QObject::connect(open_folder_button, &QPushButton::clicked, [&]() {
        if (!last_output.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(last_output).absolutePath()));
    });

    window.show();
    return app.exec();
}
