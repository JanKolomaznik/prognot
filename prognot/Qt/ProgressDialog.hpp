#pragma once

#include <QDialog>
#include <prognot/Qt/ProgressNotifier.hpp>

namespace Ui {
	class ProgressDialog;
}

namespace prognot {
namespace qt {

class ProgressDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ProgressDialog(QWidget *parent = nullptr);

	~ProgressDialog();

	void
	setText(const QString &text);

	QtProgressNotifier *
	progressNotifier() const;
protected:
	std::unique_ptr<Ui::ProgressDialog> mUi;
};

} // namespace qt
}  // namespace prognot

