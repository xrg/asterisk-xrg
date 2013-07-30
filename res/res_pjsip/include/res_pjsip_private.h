/*
 * res_pjsip.h
 *
 *  Created on: Jan 25, 2013
 *      Author: mjordan
 */

#ifndef RES_SIP_PRIVATE_H_
#define RES_SIP_PRIVATE_H_

struct ao2_container;

/*!
 * \brief Initialize the configuration for res_pjsip
 */
int ast_res_pjsip_initialize_configuration(void);

/*!
 * \brief Annihilate the configuration objects
 */
void ast_res_pjsip_destroy_configuration(void);

/*!
 * \brief Reload the configuration
 */
int ast_res_pjsip_reload_configuration(void);

/*!
 * \brief Initialize OPTIONS request handling.
 *
 * XXX This currently includes qualifying peers. It shouldn't.
 * That should go into a registrar. When that occurs, we won't
 * need the reload stuff.
 *
 * \param reload Reload options handling
 *
 * \retval 0 on success
 * \retval other on failure
 */
int ast_res_pjsip_init_options_handling(int reload);

/*!
 * \brief Initialize transport storage for contacts.
 *
 * \retval 0 on success
 * \retval other on failure
 */
int ast_res_pjsip_init_contact_transports(void);

/*!
 * \brief Initialize outbound authentication support
 *
 * \retval 0 Success
 * \retval non-zero Failure
 */
int ast_sip_initialize_outbound_authentication(void);

/*!
 * \brief Initialize system configuration
 *
 * \retval 0 Success
 * \retval non-zero Failure
 */
int ast_sip_initialize_system(void);

/*!
 * \brief Initialize global configuration
 *
 * \retval 0 Success
 * \retval non-zero Failure
 */
int ast_sip_initialize_global(void);

#endif /* RES_SIP_PRIVATE_H_ */
