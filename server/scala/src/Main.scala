import rts.Server

//import rts.PythonUnitController
import scala.collection.mutable.ArrayBuffer
import scala.collection.mutable.Buffer

import rts.JythonTest

object Main  {
  def main( args: Array[String] ) {

    var factory : JythonTest = new JythonTest( "building", "Building" )

    //var buildingObj : Object = factory.createObject() 

    //println( "class: " +  buildingObj.getClass )

    //var building : BuildingType = buildingObj.asInstanceOf[BuildingType]
    //var building : BuildingType = factory.createObject();
    //building.setBuildingName("BUIDING-A")
    //building.setBuildingAddress("100 MAIN ST.")


 //   println(building.getBuildingId() + " " +
 //     building.getBuildingName() + " " +
 //     building.getBuildingAddress())


    var hwBuf :Buffer[Object] = new ArrayBuffer[Object]
    var kwBuf :Buffer[String] = new ArrayBuffer[String]
    hwBuf += "myTransmitter"
    kwBuf += "transmitter"
    factory.runFwd( hwBuf, kwBuf )

    //(new Server).run
  }
}



