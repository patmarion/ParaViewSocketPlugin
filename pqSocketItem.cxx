/*=========================================================================

   Program: ParaView
   Module:    pqSocketItem.cxx

   Copyright (c) 2005-2008 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2. 

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#include "pqSocketItem.h"
#include "pqSocketHandler.h"

#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>


//-----------------------------------------------------------------------------
class pqSocketItem::pqInternal
{
public:

  pqInternal()
    {
    this->TcpServer = 0;
    this->TcpSocket = 0;
    this->Handler = 0;
    }

  QTcpServer* TcpServer;
  QTcpSocket* TcpSocket;

  QComboBox*     TypeCombo;
  QLineEdit*     PortEdit;
  QLineEdit*     HostEdit;
  QPushButton*   StatusButton;

  pqSocketHandler* Handler;
};

//-----------------------------------------------------------------------------
pqSocketItem::pqSocketItem(QObject* parent) : QObject(parent)
{
  this->Internal = new pqInternal;

  this->Internal->TypeCombo = new QComboBox;
  this->Internal->TypeCombo->addItem("client");
  this->Internal->TypeCombo->addItem("server");
  this->Internal->HostEdit = new QLineEdit("localhost");
  this->Internal->PortEdit = new QLineEdit("9000");
  this->Internal->StatusButton = new QPushButton();
  this->Internal->StatusButton->setMinimumWidth(100);
  this->Internal->StatusButton->setCheckable(true);

  this->connect(this->Internal->TypeCombo, SIGNAL(currentIndexChanged(int)), SLOT(onTypeChanged()));
  this->connect(this->Internal->StatusButton, SIGNAL(clicked()), SLOT(onStatusClicked()));

  this->onTypeChanged();
}

//-----------------------------------------------------------------------------
pqSocketItem::~pqSocketItem()
{
  delete this->Internal;
}

//-----------------------------------------------------------------------------
void pqSocketItem::addWidgetsToLayout(QGridLayout* layout)
{
  int row = layout->rowCount();

  layout->addWidget(this->Internal->TypeCombo, row, 0);
  layout->addWidget(this->Internal->HostEdit, row, 1);
  layout->addWidget(this->Internal->PortEdit, row, 2);
  layout->addWidget(this->Internal->StatusButton, row, 3);
}

//-----------------------------------------------------------------------------
void pqSocketItem::setHandler(pqSocketHandler* handler)
{
  this->Internal->Handler = handler;
}

//-----------------------------------------------------------------------------
void pqSocketItem::onTypeChanged()
{
  if (this->Internal->TypeCombo->currentIndex() == 0)
    {
    this->Internal->HostEdit->setEnabled(true);
    this->Internal->StatusButton->setText("Connect");
    }
  else
    {
    this->Internal->HostEdit->setEnabled(false);
    this->Internal->StatusButton->setText("Listen");
    }
}

//-----------------------------------------------------------------------------
bool pqSocketItem::openListeningSocket()
{
  QString portString = this->Internal->PortEdit->text();

  bool portOk;
  int port = portString.toInt(&portOk);
  if (!portOk)
    {
    QMessageBox::critical(0, "Invalid port",
      QString("The string '%1' is not a valid port number.").arg(portString));
    return false;
    }

  this->Internal->TcpServer = new QTcpServer(this);
  this->connect(this->Internal->TcpServer, SIGNAL(newConnection()), SLOT(onNewConnection()));

  bool success = this->Internal->TcpServer->listen(QHostAddress::Any, port);
  if (!success)
    {
    delete this->Internal->TcpServer;
    this->Internal->TcpServer = 0;

    QMessageBox::critical(0, "Socket error",
      QString("Failed to open a listening socket on port %1.").arg(port));
    }

  return success;
}

//-----------------------------------------------------------------------------
bool pqSocketItem::connectToHost()
{
  QString portString = this->Internal->PortEdit->text();

  bool portOk;
  int port = portString.toInt(&portOk);
  if (!portOk)
    {
    QMessageBox::critical(0, "Invalid port",
      QString("The string '%1' is not a valid port number.").arg(portString));
    return false;
    }

  QString hostString = this->Internal->HostEdit->text();
  if (hostString.isEmpty())
    {
    QMessageBox::critical(0, "Invalid host",
      QString("The host string is empty."));
    return false;
    }

  this->Internal->TcpSocket = new QTcpSocket(this);
  this->Internal->TcpSocket->connectToHost(hostString, port);
  bool success = this->Internal->TcpSocket->waitForConnected(1000);

  if (!success)
    {
    delete this->Internal->TcpSocket;
    this->Internal->TcpSocket = 0;

    QMessageBox::critical(0, "Socket error",
      QString("Failed to connect to %1 on port %2.").arg(hostString).arg(port));
    }
  else
    {
    this->Internal->Handler->setSocket(this->Internal->TcpSocket);
    this->Internal->Handler->onSocketOpened();
    this->connect(this->Internal->TcpSocket, SIGNAL(readyRead()), SLOT(onSocketReadReady()));
    this->connect(this->Internal->TcpSocket, SIGNAL(disconnected()), SLOT(onSocketClosed()));
    }

  return success;
}

//-----------------------------------------------------------------------------
void pqSocketItem::onStatusClicked()
{
  bool buttonIsChecked = this->Internal->StatusButton->isChecked();

  if (this->Internal->TypeCombo->currentIndex() == 0)
    {
    // client

    if (!buttonIsChecked)
      {
      if (this->Internal->TcpSocket)
        {
        this->Internal->TcpSocket->close();
        this->Internal->TcpSocket = 0;
        }

      this->Internal->StatusButton->setText("Connect");
      this->setWidgetsEnabled(true);
      }
    else
      {
      bool success = this->connectToHost();
      if (success)
        {
        this->setWidgetsEnabled(false);
        this->Internal->StatusButton->setText("Connected");
        }
      else
        {
        this->Internal->StatusButton->setChecked(false);
        }
      }

    }
  else
    {
    // server

    if (!buttonIsChecked)
      {
      if (this->Internal->TcpSocket)
        {
        this->Internal->TcpSocket->close();
        this->Internal->TcpSocket = 0;
        }
      if (this->Internal->TcpServer)
        {
        this->Internal->TcpServer->close();
        this->Internal->TcpServer->deleteLater();
        this->Internal->TcpServer = 0;
        }

      this->Internal->StatusButton->setText("Listen");
      this->setWidgetsEnabled(true);
      }
    else
      {
      bool success = this->openListeningSocket();
      if (success)
        {
        this->setWidgetsEnabled(false);
        this->Internal->StatusButton->setText("Waiting");
        }
      else
        {
        this->Internal->StatusButton->setChecked(false);
        }
      }
    }

}

//-----------------------------------------------------------------------------
void pqSocketItem::setWidgetsEnabled(bool enabled)
{
  bool isClient = this->Internal->TypeCombo->currentIndex() == 0;
  this->Internal->TypeCombo->setEnabled(enabled);
  this->Internal->PortEdit->setEnabled(enabled);
  this->Internal->HostEdit->setEnabled(enabled && isClient);
}

//-----------------------------------------------------------------------------
void pqSocketItem::onNewConnection()
{
  this->Internal->TcpSocket = this->Internal->TcpServer->nextPendingConnection();
  if (this->Internal->TcpSocket)
    {
    this->Internal->Handler->setSocket(this->Internal->TcpSocket);
    this->Internal->Handler->onSocketOpened();
    this->connect(this->Internal->TcpSocket, SIGNAL(readyRead()), SLOT(onSocketReadReady()));
    this->connect(this->Internal->TcpSocket, SIGNAL(disconnected()), SLOT(onSocketClosed()));
    this->Internal->TcpServer->close();
    this->Internal->StatusButton->setText("Connected");
    }
}

//-----------------------------------------------------------------------------
void pqSocketItem::onSocketClosed()
{
  this->Internal->Handler->setSocket(0);
  this->Internal->Handler->onSocketClosed();

  this->Internal->StatusButton->setChecked(false);
  this->onStatusClicked();
}

//-----------------------------------------------------------------------------
void pqSocketItem::onSocketReadReady()
{
  this->Internal->Handler->onSocketReadReady();
}
