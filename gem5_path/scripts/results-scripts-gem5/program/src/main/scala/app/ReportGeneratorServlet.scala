package app

import views.html
import org.scalatra._
import org.scalatra.forms._
import org.scalatra.util.RicherString._

import javax.servlet.http.HttpServletRequest
import model._
import org.scalatra.i18n.I18nSupport

trait ViewMethods {
  def report: Report

  def urlViewReport(edit: Boolean): String
  def urlRawViewListAll(tipe: String): String
  def urlViewAvailableCoords: String
  def urlViewListing(lid: String, edit: Boolean): String
  def urlViewListingSection(lid: String, index: String): String
  def urlViewListingPlot(lid: String, index: String, column: String, format: String): String
  def urlViewListingTSV(lid: String): String
  def urlViewListingSectionTSV(lid: String, index: String): String
  def urlViewOutliersLog: String
  def urlNewReport(edit: Boolean): String
  def urlReportSaveListing(lid: String): String
  def urlReportEditListingChangeSource(lid: String): String
  def urlReportEditListingIndexSetUse(lid: String, coord: String, use: String): String
  def urlReportEditListingIndexSetFilteringSomeValues(lid: String, coord: String): String
  def urlReportEditListingIndexSetFilteringAllValues(lid: String, coord: String): String
  def urlReportEditListingAddIndex(lid: String): String
  def urlReportEditListingRemoveIndex(lid: String, coord: String): String
  def urlReportEditListingIndexSetPosition(lid: String, coord: String, position: Int): String
  def urlReportEditListingResultSetOptions(lid: String, coord: String): String
  def urlReportEditListingAddResult(lid: String): String
  def urlReportEditListingRemoveResult(lid: String, coord: String): String
  def urlReportEditListingResultSetPosition(lid: String, coord: String, position: Int): String

  def show(c: Any): String = {
    import repscr.points.CoordValue
    import repscr.Vwe

    c.noCoordValue match {
      case c: Double => f"$c%4g"
      case c: Vwe => show(c.value) + "±" + show(c.error)
      case (a, b) => s"(${show(a)},${show(b)})"
      case c: Iterable[Any] => c.map(show).mkString("[", ",", "]")
      case _ => c.toString
    }
  }
}

class ReportGeneratorServlet extends ScalatraServlet with FormSupport with I18nSupport with UrlGeneratorSupport with FlashMapSupport {

  class SessionInfo(r: Report) {
    private var _report: Option[Report] = None // not used if SessionInfo.keepOnlyOneReport is true
    def report: Report = _report.getOrElse(SessionInfo.sharedReport.getOrElse(sys.error("No report loaded. Probably it has been deallocated. Maybe it is being loaded now.")))
    def report_=(r: Report): Unit =
      if (SessionInfo.keepOnlyOneReport) {
        SessionInfo.sharedReport = Some(r)
        _report = None
      } else _report = Some(r)
    def forgetReport(): Unit = { // free memory
      if (SessionInfo.keepOnlyOneReport) SessionInfo.sharedReport = None
      _report = None
    }

    report = r
  }
  object SessionInfo {
    private val keepOnlyOneReport = true // keep only a shared report for all sessions, to save memory.
    private var sharedReport: Option[Report] = None // used only if keepOnlyOneReport is true

    def newSessionInfo = {
      val si = (keepOnlyOneReport, sharedReport) match {
        case (true, Some(r)) => new SessionInfo(r)
        case (false, _) | (_, None) =>
          val id = request.cookies.getOrElse("reportGenerator_gem5_insDir", "./ins")
          val ld = request.cookies.getOrElse("reportGenerator_gem5_listingsDir", "./listings/default")
          val ro = request.cookies.getOrElse("reportGenerator_gem5_removeOutliers", "false").toBoolean
          new SessionInfo(new Report(ld, Seq(id), ro))
      }
      session.setAttribute("sessionInfo", si)
      si
    }
  }

  def sessionInfo: SessionInfo =
    if (session.contains("sessionInfo")) {
      session("sessionInfo").asInstanceOf[SessionInfo]
    } else {
      SessionInfo.newSessionInfo
    }

  object forms {
    case class NewReport(listingsDirectory: String, inputsDirectory: String, removeOutliers: Boolean)
    val newReport = mapping(
      "listingsDirectory" -> label("Listings directory", text(required)),
      "inputsDirectory" -> label("Inputs directory", text(required)),
      "removeOutliers" -> label("Remove outliers", boolean())
    )(NewReport.apply)

    case class ListingSave(name: String)
    val listingSave = mapping(
      "name" -> label("Name", text(required)),
    )(ListingSave.apply)

    case class ListingChangeSource(name: String)
    val listingChangeSource = mapping(
      "name" -> label("Name", text(required)),
    )(ListingChangeSource.apply)

    case class ListingAddIndex(name: String)
    val listingAddIndex = mapping(
      "name" -> label("Name", text(required)),
    )(ListingAddIndex.apply)

    case class ListingIndexSetFilteringSomeValues(values: Seq[String])
    val listingIndexSetFilteringSomeValues = mapping(
      "values" -> list(text()),
    )(ListingIndexSetFilteringSomeValues.apply)

    case class ListingResultSetOptions(normalized: Boolean, style: String)
    val listingResultSetOptions = mapping(
      "normalized" -> label("Normalize", boolean()),
      "style" -> label("Inputs directory", text(required)),
    )(ListingResultSetOptions.apply)

    case class ListingAddResult(name: String)
    val listingAddResult = mapping(
      "name" -> label("Name", text(required)),
    )(ListingAddIndex.apply)
  }

  implicit object viewMethods extends ViewMethods {
    def report = sessionInfo.report
    def urlViewReport(edit: Boolean) = url(viewReportRoute, "edit" -> edit.toString)
    def urlRawViewListAll(tipe: String) = url(rawViewListAllRoute, "type" -> tipe)
    def urlViewAvailableCoords = url(viewAvailableCoordsRoute)
    def urlViewListing(lid: String, edit: Boolean) = url(viewListingRoute, "lid" -> lid, "edit" -> edit.toString)
    def urlViewListingSection(lid: String, index: String) = url(viewListingSectionRoute, "lid" -> lid, "index" -> index)
    def urlViewListingPlot(lid: String, index: String, column: String, format: String): String = url(viewListingPlotRoute, "lid" -> lid, "index" -> index, "column" -> column, "format" -> format)
    def urlViewListingTSV(lid: String): String = url(viewListingTSVRoute, "lid" -> lid)
    def urlViewListingSectionTSV(lid: String, index: String) = url(viewListingSectionTSVRoute, "lid" -> lid, "index" -> index)
    def urlViewOutliersLog = url(viewOutliersLogRoute)
    def urlNewReport(edit: Boolean) = url(newReportRoute, "edit" -> edit.toString)
    def urlReportSaveListing(lid: String) = url(reportSaveListingRoute, "lid" -> lid)
    def urlReportEditListingChangeSource(lid: String) = url(reportEditListingChangeSourceRoute, "lid" -> lid)
    def urlReportEditListingIndexSetUse(lid: String, coord: String, use: String) = url(reportEditListingIndexSetUse, "lid" -> lid, "coord" -> coord, "use" -> use)
    def urlReportEditListingIndexSetFilteringSomeValues(lid: String, coord: String) = url(reportEditListingIndexSetFilteringSomeValues, "lid" -> lid, "coord" -> coord)
    def urlReportEditListingIndexSetFilteringAllValues(lid: String, coord: String) = url(reportEditListingIndexSetFilteringAllValues, "lid" -> lid, "coord" -> coord)
    def urlReportEditListingAddIndex(lid: String) = url(reportEditListingAddIndex, "lid" -> lid)
    def urlReportEditListingRemoveIndex(lid: String, coord: String) = url(reportEditListingRemoveIndex, "lid" -> lid, "coord" -> coord)
    def urlReportEditListingIndexSetPosition(lid: String, coord: String, position: Int) = url(reportEditListingIndexSetPosition, "lid" -> lid, "coord" -> coord, "position" -> position.toString)
    def urlReportEditListingResultSetOptions(lid: String, coord: String) = url(reportEditListingResultSetOptions, "lid" -> lid, "coord" -> coord)
    def urlReportEditListingAddResult(lid: String) = url(reportEditListingAddResult, "lid" -> lid)
    def urlReportEditListingRemoveResult(lid: String, coord: String) = url(reportEditListingRemoveResult, "lid" -> lid, "coord" -> coord)
    def urlReportEditListingResultSetPosition(lid: String, coord: String, position: Int) = url(reportEditListingResultSetPosition, "lid" -> lid, "coord" -> coord, "position" -> position.toString)
  }

  def withListing(lid: String)(body: Listing => Any) =
    sessionInfo.report.listings.get(lid) match {
      case Some(listing) => body(listing)
      case None => NotFound(html.error(s"Listing not found: $lid"))
    }

  def withListingSection(lid: String, index: String)(body: (Listing, Listing.SectionIndex) => Any) =
    withListing(lid) { listing =>
      listing.sectionIndexFromString(index) match {
        case Some(sec) => body(listing, sec)
        case None => NotFound(html.error(s"Section not found: $index"))
      }
    }

  /* Routes: */

  val newReportRoute = post("/newReport") {
    validate(forms.newReport)(
      errors => BadRequest(html.error(s"Invalid data:\n${errors.mkString("\n")}")),
      form => {
        sessionInfo.forgetReport()
        sessionInfo.report = new Report(form.listingsDirectory, Seq(form.inputsDirectory), form.removeOutliers)
        cookies.set("reportGenerator_gem5_insDir", form.inputsDirectory)
        cookies.set("reportGenerator_gem5_listingsDir", form.listingsDirectory)
        cookies.set("reportGenerator_gem5_removeOutliers", form.removeOutliers.toString)
        flash("message") = "Simulations and listings reloaded.\n"
        redirect(url(viewReportRoute, "edit" -> params.getAsOrElse("edit", "false")))
      }
    )
  }

  val viewReportRoute = get("/viewReport") {
    var msg = flash.get("message").map(_.toString).getOrElse("")
    if (!new java.io.File(sessionInfo.report.listingsDir).isDirectory) msg = msg + "Warning: The listings directory does not exist or is not a directory. You will not be able to save changes.\n"
    if (!sessionInfo.report.inDirs.map(new java.io.File(_)).forall(_.isDirectory)) msg = msg + "Warning: The simulations directory does not exist or is not a directory.\n"
    html.viewReport(msg, params.getAsOrElse("edit", false))
  }

  val rawViewListAllRoute = get("/viewRawList/:type") {
    val msg = flash.get("message").map(_.toString).getOrElse("")
    val tipe = params("type")
    html.viewRawList(
      tipe match {
        case "mixes" => sessionInfo.report.all_mixes
        case "sims" => sessionInfo.report.simulations
      },
      message = msg)
  }

  val viewAvailableCoordsRoute = get("/viewAvailableCoords") {
    val msg = flash.get("message").map(_.toString).getOrElse("")
    html.documentCoords(message = msg)
  }

  val viewListingRoute = get("/viewListing/index/:lid") {
    val msg = flash.get("message").map(_.toString).getOrElse("")
    val lid = params("lid")
    withListing(lid) { listing =>
      html.viewListing(lid, listing,
        showSectionIndex = params.getAsOrElse("showSectionIndex", true),
        showSections = params.getAsOrElse("showSections", false),
        msg,
        edit = params.getAsOrElse("edit", false))
    }
  }

  val viewListingSectionRoute = get("/viewListing/section/:lid/:index") {
    val msg = flash.get("message").map(_.toString).getOrElse("")
    val lid = params("lid")
    val index = params("index")
    withListing(lid) { listing =>
      listing.sectionIndexFromString(index) match {
        case Some(sec) =>
          html.viewListingSection(lid, listing, sec,
            message = msg)
        case None => NotFound(html.error("Section not found"))
      }
    }
  }

  val viewListingPlotRoute = get("/viewListing/plot/:lid/:index/:column.:format") {
    val lid = params("lid")
    val index = params("index")
    val col = params("column")
    val formatname = params("format")
    sessionInfo.report.listings.get(lid) match {
      case Some(listing) =>
        listing.sectionIndexFromString(index) match {
          case Some(sec) =>
            listing.results.find(r => r.coord.name == col) match {
              case Some(col) =>
                val p = listing.plot(sec, col)
                import repscr.plots.Format

                val fmt = formatname match {
                  case "pdf" => Format.pdf
                  case "png" => Format.png
                  case "py" => Format.python
                  case "tsv" => Format.tsv
                }
                val file = p.outputFiles(fmt)
                //response.setHeader("Content-Disposition", "attachment; filename=" + file.getName)
                fmt match {
                  case Format.python => response.setHeader("Content-Type", "text/plain;charset=utf-8")
                  case Format.tsv => response.setHeader("Content-Type", "text/tab-separated-values;charset=utf-8")
                  case _ =>
                }
                file
              case None => NotFound(html.error("Plot not found"))
            }
          case None => NotFound(html.error("Section not found"))
        }
      case None => NotFound(html.error("Listing not found"))
    }
  }

  val viewListingTSVRoute = get("/viewListing/tsv/:lid") {
    val lid = params("lid")
    withListing(lid) { listing =>
      response.setHeader("Content-Type", "text/tab-separated-values;charset=utf-8")
      listing.tsv
    }
  }

  val viewListingSectionTSVRoute = get("/viewListingSection/tsv/:lid/:index") {
    val lid = params("lid")
    val index = params("index")
    withListingSection(lid, index) { (listing, sec) =>
      response.setHeader("Content-Type", "text/tab-separated-values;charset=utf-8")
      listing.tsv(sec)
    }
  }

  val viewOutliersLogRoute = get("/viewOutliersLog") {
    response.setHeader("Content-Type", "text/plain;charset=utf-8")
    sessionInfo.report.outliers_log
  }

  val reportSaveListingRoute = post("/saveListing/:lid") {
    val lid = params("lid")
    withListing(lid) { listing =>
      validate(forms.listingSave)(
        errors => BadRequest(html.error(s"Invalid data:\n${
          errors.mkString("\n")
        }")),
        form => {
          val newName = form.name match {
            case "" => lid
            case x => x
          }
          sessionInfo.report.actions.saveListing(lid, newName)
          flash("message") = s"Listing saved as «$newName».\n"
          redirect(url(viewListingRoute, "edit" -> "true", "lid" -> newName))
        }
      )
    }
  }

  val reportEditListingChangeSourceRoute = post("/editListingChangeSource/:lid") {
    val lid = params("lid")
    withListing(lid) { listing =>
      validate(forms.listingChangeSource)(
        errors => BadRequest(html.error(s"Invalid data:\n${
          errors.mkString("\n")
        }")),
        form => {
          val srcName = form.name
          sessionInfo.report.actions.editListingChangeSource(lid, srcName)
          redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
        }
      )
    }
  }

  val reportEditListingIndexSetUse = post("/editListingIndexSetUse/:lid/:coord/:use") {
    val lid = params("lid")
    val coord = params("coord")
    val use = params("use")
    withListing(lid) { listing =>
      sessionInfo.report.actions.editListingIndexSetUse(lid, coord, use)
      redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
    }
  }

  val reportEditListingIndexSetFilteringSomeValues = post("/editListingIndexSetFilterSomeValues/:lid/:coord") {
    val lid = params("lid")
    val coord = params("coord")
    withListing(lid) { listing =>
      validate(forms.listingIndexSetFilteringSomeValues)(
        errors => BadRequest(html.error(s"Invalid data:\n${
          errors.mkString("\n")
        }")),
        form => {
          def removeTrailingCount (s: String) = s.substring(0, (s.lastIndexOf('(') - 3) max 0)
          sessionInfo.report.actions.editListingIndexSetFilteringSomeValues(lid, coord, form.values.map(removeTrailingCount): _*)
          redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
        }
      )
    }
  }

  val reportEditListingIndexSetFilteringAllValues = post("/editListingIndexSetFilterAllValues/:lid/:coord") {
    val lid = params("lid")
    val coord = params("coord")
    withListing(lid) { listing =>
      sessionInfo.report.actions.editListingIndexSetFilteringAllValues(lid, coord)
      redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
    }
  }

  val reportEditListingAddIndex = post("/editListingAddIndex/:lid") {
    val lid = params("lid")
    withListing(lid) { listing =>
      validate(forms.listingAddIndex)(
        errors => BadRequest(html.error(s"Invalid data:\n${
          errors.mkString("\n")
        }")),
        form => {
          sessionInfo.report.actions.editListingAddIndex(lid, form.name)
          redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
        }
      )
    }
  }

  val reportEditListingRemoveIndex = post("/editListingRemoveIndex/:lid/:coord") {
    val lid = params("lid")
    val coord = params("coord")
    withListing(lid) { listing =>
      sessionInfo.report.actions.editListingRemoveIndex(lid, coord)
      redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
    }
  }

  val reportEditListingIndexSetPosition = post("/editListingIndexSetPosition/:lid/:coord/:position") {
    val lid = params("lid")
    val coord = params("coord")
    val pos = params("position").toInt
    withListing(lid) { listing =>
      sessionInfo.report.actions.editListingIndexSetPosition(lid, coord, pos)
      redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
    }
  }

  val reportEditListingResultSetOptions = post("/editListingResultSetOptions/:lid/:coord") {
    val lid = params("lid")
    val coord = params("coord")
    withListing(lid) { listing =>
      validate(forms.listingResultSetOptions)(
        errors => BadRequest(html.error(s"Invalid data:\n${
          errors.mkString("\n")
        }")),
        form => {
          sessionInfo.report.actions.editListingResultSetOptions(lid, coord, form.normalized, form.style)
          redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
        }
      )
    }
  }

  val reportEditListingAddResult = post("/editListingAddResult/:lid") {
    val lid = params("lid")
    withListing(lid) { listing =>
      validate(forms.listingAddResult)(
        errors => BadRequest(html.error(s"Invalid data:\n${
          errors.mkString("\n")
        }")),
        form => {
          sessionInfo.report.actions.editListingAddResult(lid, form.name)
          redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
        }
      )
    }
  }

  val reportEditListingRemoveResult = post("/editListingRemoveResult/:lid/:coord") {
    val lid = params("lid")
    val coord = params("coord")
    withListing(lid) { listing =>
      sessionInfo.report.actions.editListingRemoveResult(lid, coord)
      redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
    }
  }

  val reportEditListingResultSetPosition = post("/editListingResultSetPosition/:lid/:coord/:position") {
    val lid = params("lid")
    val coord = params("coord")
    val pos = params("position").toInt
    withListing(lid) { listing =>
      sessionInfo.report.actions.editListingResultSetPosition(lid, coord, pos)
      redirect(url(viewListingRoute, "edit" -> "true", "lid" -> lid))
    }
  }

  get("/") {
    redirect(url(viewReportRoute))
  }
}
