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
#include <QDataStream>
#include <QTimer>

#include "pqApplicationCore.h"
#include "pqRenderView.h"
#include "pqServerManagerModel.h"

#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkPVRenderView.h>
#include <vtkRenderWindow.h>

#include <vtkRendererCollection.h>
#include <vtkRenderer.h>

#include <vtkWebGLExporter.h>
#include <vtkWebGLObject.h>

#include <vtkNew.h>

//-----------------------------------------------------------------------------
class pqPythonSocketHandler::pqInternal
{
public:

  PyObject* Callback;

  vtkNew<vtkWebGLExporter> Exporter;

  pqRenderView* RenderView;
  vtkSMRenderViewProxy* ViewProxy;

  struct CameraStateStruct {

    float Position[3];
    float FocalPoint[3];
    float ViewUp[3];

  };

  CameraStateStruct CameraState;

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
void pqPythonSocketHandler::initScene()
{

  pqServerManagerModel* smModel = pqApplicationCore::instance()->getServerManagerModel();

  QList<pqRenderView*> views = smModel->findItems<pqRenderView*>();

  if (views.empty())
    {
    return;
    }


  vtkSMRenderViewProxy* viewProxy = views[0]->getRenderViewProxy();
  this->Internal->RenderView = views[0];
  this->Internal->ViewProxy = viewProxy;

  viewProxy->Update();
  vtkRenderWindow* renderView = vtkPVRenderView::SafeDownCast(viewProxy->GetClientSideView())->GetRenderWindow();

  //printf("view: %p\n", renderView);
  //printf("view size: %d %d\n", renderView->GetRenderers()->GetFirstRenderer()->GetSize()[0], renderView->GetRenderers()->GetFirstRenderer()->GetSize()[1]);

  this->Internal->Exporter->parseScene(renderView->GetRenderers(), "\"myview\"", VTK_PARSEALL);
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::sendSceneInfo()
{

  //printf("id: %s\n", exporter->GetId());
  const char* metadata = this->Internal->Exporter->GenerateMetadata();

  unsigned long long length = strlen(metadata);

  std::cout << "stream length: " << length << std::endl;

  this->socket()->write(reinterpret_cast<char*>(&length), sizeof(length));
  this->socket()->write(metadata, strlen(metadata));
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::sendObjects()
{
  for (int i = 0; i < this->Internal->Exporter->GetNumberOfObjects(); ++i)
    {
    vtkWebGLObject* obj = this->Internal->Exporter->GetObject(i);
    if (true || obj->HasChanged())
      {

      //__int64 objectId = _atoi64(obj->GetId().c_str());
      //std::cout << "sending object id: " << objectId << std::endl;
      //this->socket()->write(reinterpret_cast<char*>(&objectId), sizeof(objectId));

      for(int partIndex = 0; partIndex < obj->GetNumberOfParts(); ++partIndex)
        {
        unsigned long long length = obj->GetBinarySize(partIndex);
        std::cout << "part stream length: " << length << std::endl;
        this->socket()->write(reinterpret_cast<char*>(&length), sizeof(length));
        this->socket()->write(reinterpret_cast<char*>(obj->GetBinaryData(partIndex)), length);
        }
      }
    }
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::receiveCameraState()
{
  //printf("receiving %d bytes\n", sizeof(this->Internal->CameraState));


  this->socket()->read(reinterpret_cast<char*>(&this->Internal->CameraState), sizeof(this->Internal->CameraState));


  double pos[3] = {this->Internal->CameraState.Position[0],this->Internal->CameraState.Position[1],this->Internal->CameraState.Position[2]};
  double focal[3] = {this->Internal->CameraState.FocalPoint[0],this->Internal->CameraState.FocalPoint[1],this->Internal->CameraState.FocalPoint[2]};
  double viewup[3] = {this->Internal->CameraState.ViewUp[0],this->Internal->CameraState.ViewUp[1],this->Internal->CameraState.ViewUp[2]};


  vtkSMPropertyHelper(this->Internal->ViewProxy, "CameraPosition").Set(pos, 3);
  vtkSMPropertyHelper(this->Internal->ViewProxy, "CameraFocalPoint").Set(focal, 3);
  vtkSMPropertyHelper(this->Internal->ViewProxy, "CameraViewUp").Set(viewup, 3);
  this->Internal->RenderView->render();


  if (this->socket()->bytesAvailable())
    {
    QTimer::singleShot(0, this, SLOT(onSocketReadReady()));
    }

  //printf("pos: %f %f %f\n", this->Internal->CameraState.Position[0],this->Internal->CameraState.Position[1],this->Internal->CameraState.Position[2]);
  //printf("view up: %f %f %f\n", this->Internal->CameraState.ViewUp[0],this->Internal->CameraState.ViewUp[1],this->Internal->CameraState.ViewUp[2]);
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::onSocketOpened()
{
  this->initScene();
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::onSocketClosed()
{
}

//-----------------------------------------------------------------------------
void pqPythonSocketHandler::onSocketReadReady()
{
  printf("onSocketReady\n");


  int command;
  this->socket()->read(reinterpret_cast<char*>(&command), sizeof(int));

  std::cout << "got command: " << command << std::endl;


  if (command == 1)
    {
    printf("send scene info\n");
    this->sendSceneInfo();
    }
  else if (command == 2)
    {
    printf("send objects\n");
    this->sendObjects();
    }
  else if (command == 3)
    {
    printf("receive camera state\n");
    this->receiveCameraState();
    }


    //double centerOfRotation[3];
    //vtkSMPropertyHelper(viewProxy, "CenterOfRotation").Get(centerOfRotation, 3);

  /*
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
  */
}
