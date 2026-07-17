
sigfm_sources = ['sigfm.cpp']

opencv = dependency('opencv5', required: false, version: '>=5.0.0')
if not opencv.found()
  opencv = dependency('opencv4', required: true, version: '>=4.5.0')
endif
doctest = dependency('doctest', required: true, version: ['>=2.0.0', '<3.0.0'])

libsigfm = static_library('sigfm',
        sigfm_sources,
        dependencies: [opencv],
)
sigfm_tests = executable('sigfm-tests', ['./tests.cpp'], dependencies: [doctest, opencv], link_with: [libsigfm])
