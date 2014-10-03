
#include "prognot/Qt/ProgressDialog.hpp"

#include "ui_ProgressDialog.h"

namespace prognot {
namespace qt {

ProgressDialog::ProgressDialog(QWidget *parent)
	: QDialog(parent)
	, mUi(new Ui::ProgressDialog())
{
	mUi->setupUi(this);

	QObject::connect(
		progressNotifier(),
		&QtProgressNotifier::finished,
		this,
				&QDialog::accept);
}

ProgressDialog::~ProgressDialog()
{}

QtProgressNotifier *
ProgressDialog::progressNotifier() const {
	return mUi->mProgressBar->progressNotifier();
}

void ProgressDialog::setText(const QString &text) {
	mUi->description_label_->setText(text);
}

} // namespace qt
}  // namespace prognot



