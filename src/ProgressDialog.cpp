#include <prognot/Qt/ProgressDialog.hpp>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
namespace prognot::qt {
ProgressDialog::ProgressDialog(QWidget* parent) : QDialog(parent),
    label_(new QLabel(this)), bar_(new ProgressBar(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(label_);
    layout->addWidget(bar_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &ProgressDialog::reject);
    connect(progressNotifier(), &QtProgressNotifier::stopped, this, [this](Completion result) {
        if (!rejected_ && result == Completion::succeeded) accept();
        else QDialog::reject();
    });
}
void ProgressDialog::setText(const QString& text) { label_->setText(text); }
QtProgressNotifier* ProgressDialog::progressNotifier() const noexcept { return bar_->progressNotifier(); }
ProgressNotifier ProgressDialog::makeNotifier() { return bar_->makeNotifier(); }
void ProgressDialog::reject() { rejected_ = true; progressNotifier()->cancel(); QDialog::reject(); }
} // namespace prognot::qt
