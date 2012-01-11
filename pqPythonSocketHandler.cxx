/*=========================================================================

   Program: ParaView
   Module:    pqPythonSocketHandler.cxx

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

#include <vtkPython.h>

#include "pqPythonSocketHandler.h"

#include <pqPVApplicationCore.h>
#include <pqPythonManager.h>
#include <pqPythonDialog.h>
#include <pqPythonShell.h>

#include <QTcpSocket>

//-----------------------------------------------------------------------------
class pqPythonSocketHandler::pqInternal
{
public:

  PyObject* Callback;
};

//-----------------------------------------------------------------------------
pqPythonSocketHandler::pqPythonSocketHandler(QObject* parent) : pqSocketHandler(parent)
{
  this->Internal = new pqInternal;

  pqPythonShell* shell = pqPVApplicationCore::instance()->pythonManager()->pythonShellDialog()->shell();
  shell->makeCurrent();

  PyRun_SimpleString(
    "def _handler(s):\n"
    "    try:\n"
    "        code = compile(s, '<string>', 'exec')\n"
    "        exec(code, globals())\n"
    "    except:\n"
    "        import traceback\n"
    "        traceback.print_exc()\n");

  PyObject* mainModule = PyImport_AddModule("__main__");
  PyObject* mainDict = PyModule_GetDict(mainModule);
  this->Internal->Callback = PyDict_GetItemString(mainDict, "_handler");
  Py_INCREF(this->Internal->Callback);

  shell->releaseControl();
}

//-----------------------------------------------------------------------------
pqPythonSocketHandler::~pqPythonSocketHandler()
{
  Py_DECREF(this->Internal->Callback);
  delete this->Internal;
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::onSocketOpened()
{
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::onSocketClosed()
{
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::onSocketReadReady()
{
  QByteArray bytes = this->socket()->readAll();

  pqPythonShell* shell = pqPVApplicationCore::instance()->pythonManager()->pythonShellDialog()->shell();
  shell->makeCurrent();

  PyObject* returnValue = PyObject_CallFunction(this->Internal->Callback,
    const_cast<char*>("s#"), bytes.data(), bytes.length());

  if (returnValue && PyString_Check(returnValue))
    {
    char* buffer;
    Py_ssize_t bufferLength;
    if (!PyString_AsStringAndSize(returnValue, &buffer, &bufferLength))
      {
      this->socket()->write(buffer, bufferLength);
      }
    Py_DECREF(returnValue);
    }

  shell->releaseControl();
}
