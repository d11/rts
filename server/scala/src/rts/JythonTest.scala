package rts
import com.weiglewilczek.slf4s._
import org.python.core.Py;
import org.python.core.PyObject;
import org.python.core.PySystemState;
import scala.collection.mutable._

//import scala.reflect._

//object JythonTest {
//
//}

//class JythonTest[I <: Class]( moduleName : String, className : String, implicit val man: Manifest[I] )  extends Object with Logging {
class JythonTest( moduleName : String, className : String )  extends Object with Logging {

  //def getOne: I = man.erasure.newInstance.asInstanceOf[I]
//  def companion = JythonTest

  val state : PySystemState = new PySystemState
  val importer : PyObject = state.getBuiltins().__getitem__(Py.newString("__import__"));
  val module   : PyObject = importer.__call__(Py.newString(moduleName));

  val run : PyObject = module.__getattr__("run")

    //val klass    : PyObject = module.__getattr__(className);
  //logger.debug("module=" + module + ",class=" + klass);

  /*
  def createObject() : BuildingType = {
    return klass.__call__().__tojava__(classOf[BuildingType]).asInstanceOf[BuildingType];
  }


  def createObject( arg1 : Object ) : Object = {
    return klass.__call__( Py.java2py( arg1 ) ).__tojava__(classOf[BuildingType]  );
  }
  */

  /*
  def createObject(Object args[], String keywords[] ) : Object {
    PyObject convertedArgs[] = new PyObject[args.length];
    for (int i = 0; i < args.length; i++ ) {
      convertedArgs[i] = Py.java2py(args[i] );
    }

    return klass.__call__( convertedArgs, keywords ).__tojava__( interfaceType );
  }*/


  def runFwd( args : Buffer[Object], keywords : Buffer[String] ) = {
    var convertedArgs : Array[PyObject] = args.toArray.map( Py.java2py )

//    for ( i <- 0 until (args.length - 1) ) {
//      convertedArgs(i) = Py.java2py(args(i) )
//    }

    var convertedKws : Array[String] = keywords.toArray


    run.__call__( convertedArgs, convertedKws )
  }


}

