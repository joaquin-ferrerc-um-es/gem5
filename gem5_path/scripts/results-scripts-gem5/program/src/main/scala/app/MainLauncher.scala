package app

import org.eclipse.jetty.server.Server
import org.eclipse.jetty.servlet.DefaultServlet
import org.eclipse.jetty.webapp.WebAppContext
import org.scalatra.servlet.ScalatraListener

import scala.annotation.tailrec

object MainLauncher {
  def main(args: Array[String]): Unit = {
    args.find(s => s == "--script" || s.startsWith("--script=")) match {
      case Some(_) =>
        scripts.Main.main(args)
      case None =>
        var port = 9000

        @tailrec
        def parseArgs(l: List[String]): Unit = l match {
          case "--port" :: p :: rest =>
            port = p.toInt
            parseArgs(rest)
          case opc :: rest if opc matches "--[^=]+=.+" =>
            parseArgs(opc.substring(0, opc.indexOf('=')) :: opc.substring(opc.indexOf('=') + 1) :: rest)
          case Nil =>
          case s =>
            Console.err.println(s"Unrecognized option: $s")
            Console.err.println("Only --port and --script are recognized.")
            sys.exit(1)
        }
        parseArgs(args.toList)

        launchServer(port).join()
    }
  }

  def launchServer(port: Int): Server = {
    val server = new Server(port)

    val context = new WebAppContext()
    context setContextPath "/"
    val resourceBase = getClass.getClassLoader.getResource("webapp") match {
      case null => "src/main/webapp" // not packaged in a JAR, use source directly
      case r => r.toExternalForm
    }
    context.setResourceBase(resourceBase)
    context.addEventListener(new ScalatraListener)
    context.addServlet(classOf[DefaultServlet], "/")

    server.setHandler(context)

    server.start()

    server
  }
}