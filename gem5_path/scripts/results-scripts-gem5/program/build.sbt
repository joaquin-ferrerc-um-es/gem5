val ScalatraVersion = "2.8.2"

organization := "ditec"

name := "ReportGenerator"

version := "0.1.0-SNAPSHOT"

scalaVersion := "2.13.12"

Compile / run / mainClass := Some("app.MainLauncher")
assembly / mainClass := Some("app.MainLauncher")

resolvers += Classpaths.typesafeReleases

libraryDependencies ++= Seq(
  "org.scalatra" %% "scalatra" % ScalatraVersion,
  //"org.scalatra" %% "scalatra-scalatest" % ScalatraVersion % "test",
  "ch.qos.logback" % "logback-classic" % "1.2.6" % "runtime",
  "org.eclipse.jetty" % "jetty-webapp" % "9.4.43.v20210629" % "container;compile",
  "javax.servlet" % "javax.servlet-api" % "3.1.0" % "provided",
  "org.scala-lang.modules" %% "scala-parallel-collections" % "1.0.3",
  "org.scalatra" %% "scalatra-forms" % ScalatraVersion,
)

scalacOptions ++= Seq("-deprecation", "-feature", "-unchecked")

enablePlugins(SbtTwirl)
enablePlugins(JettyPlugin)

Jetty / containerLibs := Seq("org.eclipse.jetty" % "jetty-runner" % "11.0.17" intransitive())
