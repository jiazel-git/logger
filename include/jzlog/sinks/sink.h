// include/jzlog/sinks/isink.h
#pragma once

#include "jzlog/core/log_level.h"
#include "jzlog/core/log_record.h"

namespace jzlog
{
namespace sinks
{

/**
 * @brief Sink ³éÏó½Ó¿Ú
 * @details ËùÓÐÊä³öÄ¿±êµÄ»ùÀà£¬¶¨ÒåÁËÈÕÖ¾Êä³öµÄ±ê×¼½Ó¿Ú
 */
class ISink {
public:
    virtual ~ISink() = default;

    // ==================== ºËÐÄ¹¦ÄÜ ====================

    /**
     * @brief Ð´ÈëÈÕÖ¾¼ÇÂ¼
     * @param record ÈÕÖ¾¼ÇÂ¼
     * @note ÕâÊÇ Sink µÄÖ÷Òª¹¦ÄÜ£¬¸ºÔð½«ÈÕÖ¾Êä³öµ½Ä¿±ê
     */
    virtual void write( const LogRecord& record ) = 0;

    /**
     * @brief Ë¢ÐÂ»º³åÇø
     * @note È·±£ËùÓÐ»º³åµÄÊý¾Ý¶¼±»Ð´ÈëÄ¿±ê
     */
    virtual void flush() = 0;

    // ==================== ¼¶±ð¹ýÂË ====================

    /**
     * @brief ÉèÖÃ×îµÍÈÕÖ¾¼¶±ð
     * @param level Ö»¼ÇÂ¼´Ë¼¶±ð¼°ÒÔÉÏµÄÈÕÖ¾
     */
    virtual void set_level( LogLevel level ) noexcept = 0;

    /**
     * @brief »ñÈ¡µ±Ç°ÉèÖÃµÄÈÕÖ¾¼¶±ð
     */
    virtual LogLevel level() const noexcept = 0;

    /**
     * @brief ¼ì²éÊÇ·ñÓ¦¸Ã¼ÇÂ¼Ö¸¶¨¼¶±ðµÄÈÕÖ¾
     * @param level Òª¼ì²éµÄÈÕÖ¾¼¶±ð
     */
    virtual bool should_log( LogLevel level ) const noexcept = 0;

    // ==================== ¿ª¹Ø¿ØÖÆ ====================

    /**
     * @brief ÆôÓÃ/½ûÓÃ Sink
     * @param enabled true ÆôÓÃ£¬false ½ûÓÃ
     */
    virtual void set_enabled( bool enabled ) noexcept = 0;

    /**
     * @brief ¼ì²é Sink ÊÇ·ñÆôÓÃ
     */
    virtual bool enabled() const noexcept = 0;
};

}  // namespace sinks
}  // namespace jzlog
