/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef ASPECTRATIOLABEL_H
#define ASPECTRATIOLABEL_H

#include "installer_global.h"

#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>

namespace QInstaller {

class INSTALLER_EXPORT AspectRatioLabel : public QLabel
{
    Q_OBJECT

public:
    explicit AspectRatioLabel(QWidget *parent = nullptr);

    int heightForWidth(int w) const override;
    QSize sizeHint() const override;

public slots:
    void setPixmap (const QPixmap &pixmap);
    void resizeEvent(QResizeEvent *event) override;

private:
    QPixmap scaledPixmap() const;

private:
    QPixmap m_pixmap;
};

} // namespace QInstaller

#endif // ASPECTRATIOLABEL_H
