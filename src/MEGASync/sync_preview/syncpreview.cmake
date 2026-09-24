set(DESKTOP_APP_SYNC_PREVIEW_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewDefs.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewProviders.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewTree.h
    ${CMAKE_CURRENT_LIST_DIR}/model/FakeSyncPreviewProvider.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewClassifier.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewPlanner.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewQueue.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewQueueStore.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewQueueFileStore.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewReconciler.h
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewPairController.h
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewDialog.h
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewPairDetailDialog.h
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewGuiFormat.h
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewRowWidget.h
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewConsequencesDialog.h
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewFakePairPicker.h
)

set(DESKTOP_APP_SYNC_PREVIEW_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewTree.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/FakeSyncPreviewProvider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewClassifier.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewPlanner.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewQueueStore.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewQueueFileStore.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewReconciler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewPairController.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewDialog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewPairDetailDialog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewRowWidget.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewConsequencesDialog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gui/SyncPreviewFakePairPicker.cpp
)

set(DESKTOP_APP_SYNC_PREVIEW_UI_FILES
    ${CMAKE_CURRENT_LIST_DIR}/gui/ui/SyncPreviewDialog.ui
    ${CMAKE_CURRENT_LIST_DIR}/gui/ui/SyncPreviewPairDetailDialog.ui
)

set_property(TARGET ${ExecutableTarget}
    APPEND PROPERTY AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_LIST_DIR}/gui/ui
)

target_sources(${ExecutableTarget}
    PRIVATE
    ${DESKTOP_APP_SYNC_PREVIEW_HEADERS}
    ${DESKTOP_APP_SYNC_PREVIEW_SOURCES}
    ${DESKTOP_APP_SYNC_PREVIEW_UI_FILES}
)

set (INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/model
    ${CMAKE_CURRENT_LIST_DIR}/gui
)

target_include_directories(${ExecutableTarget} PRIVATE ${INCLUDE_DIRECTORIES})
