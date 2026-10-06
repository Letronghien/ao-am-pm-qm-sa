build_lib_example(
  NAME manet-routing-example
  SOURCE_FILES manet-routing-example.cc
  LIBRARIES_TO_LINK
    ${libmpaodv}
    ${libaomdv}
    ${libaodv}
    ${libwifi}
    ${libmobility}
    ${libapplications}
    ${libflow-monitor}
)
