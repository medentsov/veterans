# Автотесты Qt (M12-01, M12-02, M12-03, M12-06).
#
# Включаются опцией -DVETERANS_BUILD_TESTS=ON и не влияют на обычную сборку.
# Линкуются с библиотекой приложения veterans_app, поэтому проверяют
# настоящие MainWindow/PersonDialog, а не их копии.

add_executable(veterans_tests
    test_core.cpp
)

target_link_libraries(veterans_tests PRIVATE
    veterans_app
)

set_target_properties(veterans_tests PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

add_test(NAME core COMMAND veterans_tests)
set_tests_properties(core PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
