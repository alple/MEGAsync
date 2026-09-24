set(DESKTOP_APP_SYNC_PREVIEW_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewDefs.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewProviders.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewTree.h
    ${CMAKE_CURRENT_LIST_DIR}/model/FakeSyncPreviewProvider.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewClassifier.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewPlanner.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewQueue.h
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewQueueStore.h
)

set(DESKTOP_APP_SYNC_PREVIEW_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewTree.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/FakeSyncPreviewProvider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewClassifier.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewPlanner.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model/SyncPreviewQueueStore.cpp
)

target_sources(${ExecutableTarget}
    PRIVATE
    ${DESKTOP_APP_SYNC_PREVIEW_HEADERS}
    ${DESKTOP_APP_SYNC_PREVIEW_SOURCES}
)

set (INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/model
)

target_include_directories(${ExecutableTarget} PRIVATE ${INCLUDE_DIRECTORIES})
