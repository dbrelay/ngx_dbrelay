/*
 * DB Relay is an HTTP module built on the NGiNX webserver platform which 
 * communicates with a variety of database servers and returns JSON formatted 
 * data.
 * 
 * Copyright (C) 2008-2010 Getco LLC
 * 
 * This program is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the Free 
 * Software Foundation, either version 3 of the License, or (at your option) 
 * any later version. In addition, redistributions in source code and in binary 
 * form must 
 * include the above copyright notices, and each of the following disclaimers. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT OWNERS AND CONTRIBUTORS “AS IS” 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANY COPYRIGHT OWNERS OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "dbrelay.h"
#include <stdarg.h>

void dbrelay_log(unsigned int log_level, dbrelay_request_t *request, const char *fmt, va_list args);

#ifdef CMDLINE
#define NGX_MAX_ERROR_STR 4096
#endif

void
dbrelay_log(unsigned int log_level, dbrelay_request_t *request, const char *fmt, va_list args)
{
   u_char buf[NGX_MAX_ERROR_STR];
   u_char *p;

   /* ngx_vsnprintf() does not appear to null terminate the buffer it writes
    * into and provides no length, therefore we must do this
    */
   memset(buf, 0, NGX_MAX_ERROR_STR);

#ifndef CMDLINE
   p = ngx_vsnprintf(buf, NGX_MAX_ERROR_STR, fmt, args);
   ngx_log_error(log_level, request->log, 0, "%s\n", (char *)buf);
#else
   p = vsnprintf((char *)buf, NGX_MAX_ERROR_STR, fmt, args);
   fprintf(stderr, "%s\n", (char *)buf);
#endif
}
void
dbrelay_log_debug(dbrelay_request_t *request, const char *fmt, ...)
{
   va_list  args;

   if (request->log_level > DBRELAY_LOG_LVL_DEBUG) return;

   va_start(args, fmt);
   dbrelay_log(DBRELAY_LOG_LVL_DEBUG, request, fmt, args);
   va_end(args);
}
void
dbrelay_log_info(dbrelay_request_t *request, const char *fmt, ...)
{
   va_list  args;

   if (request->log_level > DBRELAY_LOG_LVL_INFO) return;

   va_start(args, fmt);
   dbrelay_log(DBRELAY_LOG_LVL_INFO, request, fmt, args);
   va_end(args);
}
void
dbrelay_log_notice(dbrelay_request_t *request, const char *fmt, ...)
{
   va_list  args;

   if (request->log_level > DBRELAY_LOG_LVL_NOTICE) return;

   va_start(args, fmt);
   dbrelay_log(DBRELAY_LOG_LVL_NOTICE, request, fmt, args);
   va_end(args);
}
void
dbrelay_log_warn(dbrelay_request_t *request, const char *fmt, ...)
{
   va_list  args;

   if (request->log_level > DBRELAY_LOG_LVL_WARN) return;

   va_start(args, fmt);
   dbrelay_log(DBRELAY_LOG_LVL_WARN, request, fmt, args);
   va_end(args);
}
void
dbrelay_log_error(dbrelay_request_t *request, const char *fmt, ...)
{
   va_list  args;

   if (request->log_level > DBRELAY_LOG_LVL_ERROR) return;

   va_start(args, fmt);
   dbrelay_log(DBRELAY_LOG_LVL_ERROR, request, fmt, args);
   va_end(args);
}
