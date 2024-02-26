addSbtPlugin("com.typesafe.sbt" % "sbt-twirl" % "1.5.1")
//addSbtPlugin("com.typesafe.play" % "sbt-twirl" % "1.6.2")
addSbtPlugin("com.earldouglas" % "xsbt-web-plugin" % "4.2.4")

addSbtPlugin("com.eed3si9n" % "sbt-assembly" % "2.1.5")

// https://github.com/scala/bug/issues/12632
ThisBuild / libraryDependencySchemes ++= Seq(
  "org.scala-lang.modules" %% "scala-xml" % VersionScheme.Always
)
